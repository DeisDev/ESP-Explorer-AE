#include "Platform/RendererResources.h"
#include "Core/ScopeExit.h"

#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>
#include <array>

namespace ESPExplorerAE
{
    namespace
    {
        // Binding the back buffer may unbind UAVs or shader-resource aliases.
        // Preserve those bindings as well as all color/depth outputs.
        class DrawTargetState
        {
            static constexpr std::array getters{ &ID3D11DeviceContext::VSGetShaderResources, &ID3D11DeviceContext::HSGetShaderResources,
                &ID3D11DeviceContext::DSGetShaderResources, &ID3D11DeviceContext::GSGetShaderResources,
                &ID3D11DeviceContext::PSGetShaderResources, &ID3D11DeviceContext::CSGetShaderResources };
            static constexpr std::array setters{ &ID3D11DeviceContext::VSSetShaderResources, &ID3D11DeviceContext::HSSetShaderResources,
                &ID3D11DeviceContext::DSSetShaderResources, &ID3D11DeviceContext::GSSetShaderResources,
                &ID3D11DeviceContext::PSSetShaderResources, &ID3D11DeviceContext::CSSetShaderResources };
            ID3D11DeviceContext* context;
            std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> targets{};
            Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth;
            std::array<ID3D11UnorderedAccessView*, D3D11_1_UAV_SLOT_COUNT> unordered{};
            std::array<ID3D11UnorderedAccessView*, D3D11_1_UAV_SLOT_COUNT> computeUnordered{};
            std::array<std::array<ID3D11ShaderResourceView*, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT>, getters.size()> resources{};
            UINT unorderedCount;
            bool preserveResources;
            bool preserveCompute;
        public:
            DrawTargetState(ID3D11DeviceContext* value, D3D_FEATURE_LEVEL level, UINT bindFlags) :
                context(value), unorderedCount(level >= D3D_FEATURE_LEVEL_11_1 ? D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT),
                preserveResources((bindFlags & D3D11_BIND_SHADER_RESOURCE) != 0), preserveCompute((bindFlags & D3D11_BIND_UNORDERED_ACCESS) != 0)
            {
                context->OMGetRenderTargetsAndUnorderedAccessViews(static_cast<UINT>(targets.size()), targets.data(), depth.GetAddressOf(), 0, unorderedCount, unordered.data());
                if (preserveCompute) context->CSGetUnorderedAccessViews(0, unorderedCount, computeUnordered.data());
                if (preserveResources) {
                    for (std::size_t stage = 0; stage < resources.size(); ++stage)
                        (context->*getters[stage])(0, static_cast<UINT>(resources[stage].size()), resources[stage].data());
                }
            }
            DrawTargetState(const DrawTargetState&) = delete;
            DrawTargetState& operator=(const DrawTargetState&) = delete;
            ~DrawTargetState()
            {
                UINT targetCount{};
                for (UINT index = 0; index < targets.size(); ++index) if (targets[index]) targetCount = index + 1;
                std::array<UINT, D3D11_1_UAV_SLOT_COUNT> counts;
                counts.fill(static_cast<UINT>(-1)); // Preserve append/consume counters.
                if (targetCount < unorderedCount) context->OMSetRenderTargetsAndUnorderedAccessViews(targetCount, targets.data(), depth.Get(),
                    targetCount, unorderedCount - targetCount, unordered.data() + targetCount, counts.data());
                else context->OMSetRenderTargets(targetCount, targets.data(), depth.Get());
                for (auto* target : targets) if (target) target->Release();
                for (auto* view : unordered) if (view) view->Release();
                if (preserveCompute) {
                    context->CSSetUnorderedAccessViews(0, unorderedCount, computeUnordered.data(), counts.data());
                    for (auto* view : computeUnordered) if (view) view->Release();
                }
                if (preserveResources) {
                    for (std::size_t stage = 0; stage < resources.size(); ++stage) {
                        (context->*setters[stage])(0, static_cast<UINT>(resources[stage].size()), resources[stage].data());
                        for (auto* view : resources[stage]) if (view) view->Release();
                    }
                }
            }
        };
    }

    bool RendererResources::Initialize(IDXGISwapChain* chain, HWND window, bool (*prepare)(),
        void (*onReset)(), bool (*checkpoint)(RendererStage))
    {
        if (ready) return true;
        if (!chain || !window) return false;
        const auto previous = ImGui::GetCurrentContext();
        ScopeExit restore([previous] { ImGui::SetCurrentContext(previous); });
        ScopeExit rollback([this] { Release(); });
        const auto proceed = [checkpoint](RendererStage stage) { return !checkpoint || checkpoint(stage); };
        if (FAILED(chain->GetDevice(IID_PPV_ARGS(device.GetAddressOf()))) || !proceed(RendererStage::Device)) return false;
        device->GetImmediateContext(immediate.GetAddressOf());
        if (!immediate || !proceed(RendererStage::ImmediateContext)) return false;
        swapChain = chain;
        IMGUI_CHECKVERSION();
        imgui = ImGui::CreateContext();
        if (!imgui) return false;
        ImGui::SetCurrentContext(imgui);
        reset = onReset;
        if (!proceed(RendererStage::ImGuiContext)) return false;
        if ((prepare && !prepare()) || !proceed(RendererStage::Prepared)) return false;
        if (!ImGui_ImplWin32_Init(window)) return false;
        win32Ready = true;
        if (!proceed(RendererStage::Win32)) return false;
        if (!ImGui_ImplDX11_Init(device.Get(), immediate.Get())) return false;
        dx11Ready = true;
        if (!proceed(RendererStage::DX11)) return false;
        if (!ImGui_ImplDX11_CreateDeviceObjects() || !proceed(RendererStage::DeviceObjects)) return false;
        ready = true;
        rollback.Release();
        return true;
    }

    HRESULT RendererResources::RenderDrawData(ImDrawData* drawData)
    {
        if (!ready || !drawData) return E_UNEXPECTED;
        if (drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f) return S_FALSE;
        // Borrow the current buffer only for this draw. Keeping its texture/view
        // between Presents would prevent the host from resizing its swap chain.
        Microsoft::WRL::ComPtr<ID3D11Texture2D> buffer;
        auto result = swapChain->GetBuffer(0, IID_PPV_ARGS(buffer.GetAddressOf()));
        if (FAILED(result)) return result;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
        result = device->CreateRenderTargetView(buffer.Get(), nullptr, target.GetAddressOf());
        if (FAILED(result)) return result;
        D3D11_TEXTURE2D_DESC description{};
        buffer->GetDesc(&description);
        const auto previousScale = drawData->FramebufferScale;
        ScopeExit restoreScale([&] { drawData->FramebufferScale = previousScale; });
        drawData->FramebufferScale = ImVec2(static_cast<float>(description.Width) / drawData->DisplaySize.x,
            static_cast<float>(description.Height) / drawData->DisplaySize.y);

        // ImGui leaves output targets to the caller. The game may have left no
        // target or an offscreen one; neither is a destination for the overlay.
        const DrawTargetState previousTargets(immediate.Get(), device->GetFeatureLevel(), description.BindFlags);
        immediate->OMSetRenderTargets(1, target.GetAddressOf(), nullptr);
        const auto previousContext = ImGui::GetCurrentContext();
        ScopeExit restoreContext([previousContext] { ImGui::SetCurrentContext(previousContext); });
        ImGui::SetCurrentContext(imgui);
        ImGui_ImplDX11_RenderDrawData(drawData);
        return S_OK;
    }

    void RendererResources::Release()
    {
        const auto previous = ImGui::GetCurrentContext();
        if (imgui) {
            ImGui::SetCurrentContext(imgui);
            if (dx11Ready) ImGui_ImplDX11_Shutdown();
            if (win32Ready) ImGui_ImplWin32_Shutdown();
            if (reset) reset();
            ImGui::DestroyContext(imgui);
            ImGui::SetCurrentContext(previous == imgui ? nullptr : previous);
            imgui = nullptr;
        }
        reset = nullptr;
        win32Ready = dx11Ready = ready = false;
        swapChain.Reset();
        immediate.Reset();
        device.Reset();
    }
}
