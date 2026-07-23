#ifndef FILER_ACTIONS_H
#define FILER_ACTIONS_H

#include "launchelf.h"

u64 getFileSize(const char *path, const FILEINFO *file);
void time_manip(const char *path, const FILEINFO *file, char *_msg0);
void make_title_cfg(const char *path, const FILEINFO *file, char *_msg0);
int filerIsExploitProtectedPath(const char *path, const FILEINFO *file);
int filerConfirmExploitDelete(const char *path, const FILEINFO *file);
int filerConfirmExploitModify(const char *path, const FILEINFO *file);
int delete (const char *path, const FILEINFO *file);
int Rename(const char *path, const FILEINFO *file, const char *name);
int newdir(const char *path, const char *name);

#endif
