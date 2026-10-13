# gfx: one binary, one ctest test per case group. Pictures from ../assets
# (avatars, GIFs, svg/), plus the repository's own art in ../../gfx.
add_executable(gfx_tests gfx_tests.cpp)
target_link_libraries(gfx_tests PRIVATE msga_gfx)
target_compile_definitions(gfx_tests PRIVATE
    MSGA_TEST_ASSETS="${MSGA_TEST_ASSETS}"
    GFX_REPO_ART="${MSGA_ROOT}/gfx")
# gfx_tests replaces the global operator new (its allocation checks keep a
# header before each block). On Windows a replacement does not reach into
# libstdc++-6.dll, and since GCC 16 std::string's members are taken from there
# in C++20 too: a string grown by the DLL and freed here, or the reverse, is a
# corrupted heap (exit code 0xc0000374). With libstdc++ linked statically the
# one operator new serves both.
if(MINGW)
    target_link_options(gfx_tests PRIVATE -static-libstdc++)
endif()
foreach(t blend fill clip roundrect path stroke bitmap blit gradient shadow decode anim animbudget snapped icons
        paint2 svg svgsize svgfuzz cover resizeaxes scratch svglayers)
    add_test(NAME gfx_${t} COMMAND gfx_tests ${t})
endforeach()
