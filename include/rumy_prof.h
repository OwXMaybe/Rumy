#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void rumy_prof_begin(const char* tag);
void rumy_prof_end(const char* tag);

#ifdef __cplusplus
}
#endif

#if defined(RUMY_PROFILE) && RUMY_PROFILE
#define RUMY_PROF_BEGIN(tag) rumy_prof_begin(tag)
#define RUMY_PROF_END(tag) rumy_prof_end(tag)
#else
#define RUMY_PROF_BEGIN(tag)
#define RUMY_PROF_END(tag)
#endif
