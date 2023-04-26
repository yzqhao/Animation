set (
	BUILD_LIST_COMMON
    ./Third/glad/ glad_dir
    ./Third/stb_image/ stb_image_dir
    ./Third/glfw-3.3.5/src/ glfw3_dir

	./System system_dir
	./Math math_dir
	./Common common_dir

	./Samples/playback playback_dir
	./Samples/attach attach_dir
	./Samples/blend blend_dir
	./Samples/partial_blend partial_blend_dir
	#./Samples/additive_blend additive_blend_dir
	./Samples/skinning skinning_dir
)


set ( BUILD_LIST_COMMON
        ${BUILD_LIST_COMMON}
    )
	
set ( BUILD_LIST_WINDOWS
	${BUILD_LIST_COMMON}
)