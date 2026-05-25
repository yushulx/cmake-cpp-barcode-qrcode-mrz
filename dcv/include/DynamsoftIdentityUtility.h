#pragma once

#if !defined(_WIN32) && !defined(_WIN64)
	#define ID_UTILITY_API __attribute__((visibility("default")))
    #include <stddef.h>
#else
	#ifdef ID_UTILITY_EXPORTS
		#define ID_UTILITY_API __declspec(dllexport)
	#else
		#define ID_UTILITY_API __declspec(dllimport)
	#endif 
    #include <windows.h>
#endif

#include "DynamsoftCaptureVisionRouter.h"
#define ID_UTILITY_VERSION  "1.0.10.6913"

using namespace dynamsoft::dbr;
using namespace dynamsoft::basic_structures;
using namespace dynamsoft::dlr::intermediate_results;
using namespace dynamsoft::ddn::intermediate_results;

namespace dynamsoft {
    namespace id_utility {
        /**
         * The CIdentityUtilityModule class provides common functions of the identity utility module.
         */
        class ID_UTILITY_API CIdentityUtilityModule
        {
        public:
            /**
             * Gets the version of the identity utility module.
             *
             * @return Returns the version string.
             */
            static const char* GetVersion();
        };

        /**
         * The CIdentityProcessor class provides functions to process identity documents,
         * such as locating portrait zones with higher precision.
         */
        class ID_UTILITY_API CIdentityProcessor
        {
        public:
            /**
             * Finds the location of the portrait zone on an identity document.
             *
             * @param [in] scaledColourImgUnit The scaled colour image unit containing the source image.
             * @param [in] localizedTextLinesUnit The localized text lines unit containing MRZ/text regions.
             * @param [in] recognizedTextLinesUnit The recognized text lines unit for document type identification.
             * @param [in] detectedQuadsUnit The detected quads unit containing document boundaries.
             * @param [in] deskewedImageUnit The deskewed image unit for coordinate transformation.
             * @param [out] portraitZone The output quadrilateral representing the portrait zone location.
             *                           Returns an empty quadrilateral if not found.
             *
             * @return Returns 0 if successful, otherwise returns an error code.
             */
            int FindPortraitZone(const CScaledColourImageUnit* scaledColourImgUnit,
                const CLocalizedTextLinesUnit* localizedTextLinesUnit,
                const CRecognizedTextLinesUnit* recognizedTextLinesUnit,
                const CDetectedQuadsUnit* detectedQuadsUnit,
                const CDeskewedImageUnit* deskewedImageUnit,
                CQuadrilateral& portraitZone);
        };
    }
}