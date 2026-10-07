/*
 * glibc ABI 兼容 shim
 *
 * 交叉工具链 (Debian trixie, glibc 2.41) 在 aarch64 上为 fmod 提供了
 * 新符号版本 GLIBC_2.38 作为默认绑定, 而目标板 (RPi OS bookworm,
 * glibc 2.36) 只提供 GLIBC_2.17 的 fmod.
 *
 * 这里让本可执行文件自己定义 fmod: 静态库 (lua 的 math 库等) 对 fmod
 * 的引用会在静态链接阶段解析到本定义, 不再直接绑定 2.38 版本;
 * 本定义内部通过 .symver 显式绑定到旧符号 fmod@GLIBC_2.17,
 * 使最终二进制在 bookworm 上可加载.
 */
#ifdef __aarch64__

#include <math.h>

__asm__(".symver ptx_fmod_glibc217, fmod@GLIBC_2.17");
extern double ptx_fmod_glibc217(double, double);

double fmod(double x, double y)
{
    return ptx_fmod_glibc217(x, y);
}

#endif /* __aarch64__ */
