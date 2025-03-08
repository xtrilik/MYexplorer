// File: FingerprintUtils.h
// Version: 1.0 (Phase 3.1: Fingerprint Utilities extracted)
// -------------------------------------------------------------------------
// This header declares functions for composite fingerprinting.
// Functions include GetFingerprint(), CompareFingerprints(), and CompareStableAttributes().
// These functions encapsulate the window tracking logic.
// -------------------------------------------------------------------------

#ifndef FINGERPRINTUTILS_H
#define FINGERPRINTUTILS_H

#include <windows.h>
#include <string>
#include "MainWindow.h" // For TrackedWindow definition

// Captures composite fingerprint details of the given window.
TrackedWindow GetFingerprint(HWND hwnd);

// Compares two window fingerprints in detail.
bool CompareFingerprints(const TrackedWindow &twStored, const TrackedWindow &twCurrent);

// Compares stable attributes of two fingerprints (process ID and class name).
bool CompareStableAttributes(const TrackedWindow &stored, const TrackedWindow &current);

#endif // FINGERPRINTUTILS_H

// End of file: FingerprintUtils.h (Version: 1.0)
