set(COMPONENT_SRCS "sid_stream.c" "libcsidlight.c")
set(COMPONENT_ADD_INCLUDEDIRS ".")

set(COMPONENT_REQUIRES audio_pipeline)

register_component()
