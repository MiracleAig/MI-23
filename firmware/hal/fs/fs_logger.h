#pragma once

#include "hal/fs/axiom_fs.h"

namespace AxiomFS {

void appendLog(FileSystem* fs, const char* path, const char* message);
void appendBootLog(FileSystem* fs, const char* message);
void appendFsLog(FileSystem* fs, const char* message);

} // namespace AxiomFS
