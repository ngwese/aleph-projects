/* module_custom.h

   some constants should be both customized for each module,
   and preprocessed for efficiency.
   here is where such things live.
 */

#ifndef _ALEPH_MODULE_CUSTOM_H_
#define _ALEPH_MODULE_CUSTOM_H_

#include "parallel_params.h"

#define MODULE_BLOCKSIZE 16

//! Pack/unpack SPORT 24-bit words to fract32 around module_process_block
#define MODULE_AUDIO_CONVERT_24_32 1

//! Detect window overrun / buffer clash xruns (SPI readout always available)
#define MODULE_AUDIO_XRUN_DETECT 1

#define NUM_PARAMS eParamNumParams

#endif // h guard
