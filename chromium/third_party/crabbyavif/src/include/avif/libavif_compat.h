/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Functions.
#define avifAlloc crabby_avifAlloc
#define avifCropRectConvertCleanApertureBox crabby_avifCropRectConvertCleanApertureBox
#define avifDecoderCreate crabby_avifDecoderCreate
#define avifDecoderDecodedRowCount crabby_avifDecoderDecodedRowCount
#define avifDecoderDestroy crabby_avifDecoderDestroy
#define avifDecoderIsKeyframe crabby_avifDecoderIsKeyframe
#define avifDecoderNearestKeyframe crabby_avifDecoderNearestKeyframe
#define avifDecoderNextImage crabby_avifDecoderNextImage
#define avifDecoderNthImage crabby_avifDecoderNthImage
#define avifDecoderNthImageMaxExtent crabby_avifDecoderNthImageMaxExtent
#define avifDecoderNthImageTiming crabby_avifDecoderNthImageTiming
#define avifDecoderParse crabby_avifDecoderParse
#define avifDecoderRead crabby_avifDecoderRead
#define avifDecoderReadFile crabby_avifDecoderReadFile
#define avifDecoderReadMemory crabby_avifDecoderReadMemory
#define avifDecoderSetIO crabby_avifDecoderSetIO
#define avifDecoderSetIOFile crabby_avifDecoderSetIOFile
#define avifDecoderSetIOMemory crabby_avifDecoderSetIOMemory
#define avifDecoderSetSource crabby_avifDecoderSetSource
#define avifDiagnosticsClearError crabby_avifDiagnosticsClearError
#define avifFree crabby_avifFree
#define avifGetPixelFormatInfo crabby_avifGetPixelFormatInfo
#define avifIOCreateFileReader crabby_avifIOCreateFileReader
#define avifIOCreateMemoryReader crabby_avifIOCreateMemoryReader
#define avifIODestroy crabby_avifIODestroy
#define avifImageAllocatePlanes crabby_avifImageAllocatePlanes
#define avifImageCreate crabby_avifImageCreate
#define avifImageCreateEmpty crabby_avifImageCreateEmpty
#define avifImageDestroy crabby_avifImageDestroy
#define avifImageFreePlanes crabby_avifImageFreePlanes
#define avifImageIsOpaque crabby_avifImageIsOpaque
#define avifImagePlane crabby_avifImagePlane
#define avifImagePlaneHeight crabby_avifImagePlaneHeight
#define avifImagePlaneRowBytes crabby_avifImagePlaneRowBytes
#define avifImagePlaneWidth crabby_avifImagePlaneWidth
#define avifImageScale crabby_avifImageScale
#define avifImageSetViewRect crabby_avifImageSetViewRect
#define avifImageUsesU16 crabby_avifImageUsesU16
#define avifImageYUVToRGB crabby_avifImageYUVToRGB
#define avifPeekCompatibleFileType crabby_avifPeekCompatibleFileType
#define avifRGBImageSetDefaults crabby_avifRGBImageSetDefaults
#define avifRWDataFree crabby_avifRWDataFree
#define avifRWDataRealloc crabby_avifRWDataRealloc
#define avifRWDataSet crabby_avifRWDataSet
#define avifResultToString crabby_avifResultToString
// Constants.
#define AVIF_DIAGNOSTICS_ERROR_BUFFER_SIZE CRABBY_AVIF_DIAGNOSTICS_ERROR_BUFFER_SIZE
#define AVIF_FALSE CRABBY_AVIF_FALSE
#define AVIF_PLANE_COUNT_YUV CRABBY_AVIF_PLANE_COUNT_YUV
#define AVIF_REPETITION_COUNT_INFINITE CRABBY_AVIF_REPETITION_COUNT_INFINITE
#define AVIF_REPETITION_COUNT_UNKNOWN CRABBY_AVIF_REPETITION_COUNT_UNKNOWN
#define AVIF_TRUE CRABBY_AVIF_TRUE
#define DEFAULT_IMAGE_COUNT_LIMIT CRABBY_AVIF_DEFAULT_IMAGE_COUNT_LIMIT
#define DEFAULT_IMAGE_DIMENSION_LIMIT CRABBY_AVIF_DEFAULT_IMAGE_DIMENSION_LIMIT
#define DEFAULT_IMAGE_SIZE_LIMIT CRABBY_AVIF_DEFAULT_IMAGE_SIZE_LIMIT
#define MAX_AV1_LAYER_COUNT CRABBY_AVIF_MAX_AV1_LAYER_COUNT
