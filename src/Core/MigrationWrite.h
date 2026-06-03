#pragma once

#include <optional>
#include <string>

namespace ESPExplorerAE
{
    // Keep the original bytes until the backup is durable. A failed final write
    // retries publication without creating another backup or losing the first.
    template <class Backup, class Write>
    bool WriteWithMigrationBackup(std::optional<std::string>& original, Backup&& backup, Write&& write)
    {
        if (original) {
            if (!backup(*original)) return false;
            original.reset();
        }
        return write();
    }
}
