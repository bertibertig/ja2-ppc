# cmake -DPATCH_DIR=<dir> -P ApplyPatches.cmake, run in the tree to patch.
# Applies every <dir>/*.patch with patch -p1 in name order.
file(GLOB patches "${PATCH_DIR}/*.patch")
list(SORT patches)
foreach(patch_file IN LISTS patches)
    execute_process(COMMAND patch -p1 -s --forward -i "${patch_file}"
                    RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "failed to apply ${patch_file}")
    endif()
endforeach()
