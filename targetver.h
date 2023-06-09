#ifndef TARGETVER_H_FILE
#define TARGETVER_H_FILE

// // Including SDKDDKVer.h defines the highest available Windows platform.
// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.

#ifdef _WIN32

#include <WinSDKVer.h>
#define WINVER       0x0A00
#define _WIN32_WINNT 0x0A00  

#include <SDKDDKVer.h>

#endif /* _WIN32 */

#endif /* TARGETVER_H_FILE */
