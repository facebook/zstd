/*! ZDICT_trainFromBuffer_unsafe_legacy() :
    Strictly Internal use only !!
    Same as ZDICT_trainFromBuffer_legacy(), but does not control `samplesBuffer`.
    `samplesBuffer` must be followed by noisy guard band to avoid out-of-buffer reads.
    @return : size of dictionary stored into `dictBuffer` (<= `dictBufferCapacity`)
              or an error code.
*/
size_t ZDICT_trainFromBuffer_unsafe_legacy(void* dictBuffer, size_t dictBufferCapacity,
                                           const void* samplesBuffer, const size_t* samplesSizes, unsigned nbSamples,
                                           ZDICT_legacy_params_t parameters);
