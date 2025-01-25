// Copyright 2022 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_MOJO_SERVICE_CONSTANTS_H_
#define SYSTEM_API_MOJO_SERVICE_CONSTANTS_H_

namespace chromeos::mojo_services {

// Please keep alphabetized.
constexpr char kChromiumArcBridgeHost[] = "ChromiumArcBridgeHost";
constexpr char kChromiumCrosHealthdDataCollector[] =
    "ChromiumCrosHealthdDataCollector";
constexpr char kChromiumNetworkDiagnosticsRoutines[] =
    "ChromiumNetworkDiagnosticsRoutines";
constexpr char kChromiumNetworkHealth[] = "ChromiumNetworkHealth";
constexpr char kCrosCameraAppDeviceBridge[] = "CrosCameraAppDeviceBridge";
constexpr char kCrosCameraController[] = "CrosCameraController";
constexpr char kCrosCameraDiagnostics[] = "CrosCameraDiagnostics";
constexpr char kCrosCameraDiagnosticsService[] = "CrosCameraDiagnosticsService";
constexpr char kCrosCameraHalDispatcher[] = "CrosCameraHalDispatcher";
constexpr char kCrosCameraService[] = "CrosCameraService";
constexpr char kCrosDcadService[] = "CrosDcadService";
constexpr char kCrosDocumentScanner[] = "CrosDocumentScanner";
constexpr char kCrosHealthdAshEventReporter[] = "CrosHealthdAshEventReporter";
constexpr char kCrosHealthdDiagnostics[] = "CrosHealthdDiagnostics";
constexpr char kCrosHealthdEvent[] = "CrosHealthdEvent";
constexpr char kCrosHealthdProbe[] = "CrosHealthdProbe";
constexpr char kCrosHealthdRoutines[] = "CrosHealthdRoutines";
constexpr char kCrosJpegAccelerator[] = "CrosJpegAccelerator";
constexpr char kCrosMantisService[] = "CrosMantisService";
constexpr char kCrosPasspointService[] = "CrosPasspointService";
constexpr char kCrosPortalService[] = "CrosPortalService";
constexpr char kCrosSystemEventMonitor[] = "CrosSystemEventMonitor";
constexpr char kHeartdControl[] = "HeartdControl";
constexpr char kHeartdHeartbeatService[] = "HeartdHeartbeatService";
constexpr char kIioSensor[] = "IioSensor";
constexpr char kVideoCaptureDeviceInfoMonitor[] =
    "VideoCaptureDeviceInfoMonitor";
constexpr char kCrosOdmlService[] = "CrosOdmlService";
constexpr char kCrosCoralService[] = "CrosCoralService";
constexpr char kCrosEmbeddingModelService[] = "CrosEmbeddingModelService";
constexpr char kCrosSafetyService[] = "CrosSafetyService";

}  // namespace chromeos::mojo_services

#endif  // SYSTEM_API_MOJO_SERVICE_CONSTANTS_H_
