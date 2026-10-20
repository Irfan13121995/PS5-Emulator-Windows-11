#include "ps5_hle_modules.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace ps5::emu {

HleLibKernel::HleLibKernel(
    ps5::lowspec::SparseUnifiedMemory& mem,
    const std::string& host_app0_root)
    : mem_(mem)
    , host_app0_root_(host_app0_root)
{}

int32_t HleLibKernel::sceKernelAllocateDirectMemory(
    int64_t /*search_start*/,
    int64_t /*search_end*/,
    size_t len,
    size_t alignment,
    int32_t /*memory_type*/,
    int64_t* out_phys_addr)
{
    if (!out_phys_addr || len == 0 || len > ps5::lowspec::SparseUnifiedMemory::kGuestVirtualSpaceSize) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const size_t align = std::max<size_t>(alignment, ps5::lowspec::SparseUnifiedMemory::kPageSize);
    int64_t aligned = (next_direct_phys_addr_ + static_cast<int64_t>(align - 1)) & ~static_cast<int64_t>(align - 1);
    *out_phys_addr = aligned;
    next_direct_phys_addr_ = aligned + static_cast<int64_t>(len);
    return SCE_OK;
}

int32_t HleLibKernel::sceKernelMapDirectMemory(
    uint64_t* inout_vaddr,
    size_t len,
    int32_t /*prot*/,
    int32_t /*flags*/,
    int64_t /*direct_memory_start*/,
    size_t /*alignment*/)
{
    if (!inout_vaddr || len == 0) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    uint64_t vaddr = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        vaddr = (*inout_vaddr != 0) ? *inout_vaddr : next_virtual_base_;
        next_virtual_base_ = vaddr + ((len + 0xFFFFULL) & ~0xFFFFULL);
    }

    // Demand-fault the first 64 KB page in our SparseUnifiedMemory manager
    if (!mem_.TouchGuestPage(vaddr, true, 1)) {
        return SCE_KERNEL_ERROR_ENOMEM;
    }

    *inout_vaddr = vaddr;
    return SCE_OK;
}

bool HleLibKernel::ResolveSandboxedGuestPath(
    const std::string& guest_path,
    std::string& out_host_path) const
{
    // Reject null bytes and explicit parent directory traversal (`..`)
    if (guest_path.empty() ||
        guest_path.find('\0') != std::string::npos ||
        guest_path.find("..") != std::string::npos)
    {
        return false;
    }

    constexpr const char* kApp0Prefix = "/app0/";
    std::string rel = guest_path;
    if (rel.rfind(kApp0Prefix, 0) == 0) {
        rel = rel.substr(6);
    } else if (!rel.empty() && rel[0] == '/') {
        rel = rel.substr(1);
    }

    std::filesystem::path root = std::filesystem::path(host_app0_root_).lexically_normal();
    std::filesystem::path combined = (root / rel).lexically_normal();

    // Ensure resolved path remains strictly inside `root`
    auto root_str = root.string();
    auto comb_str = combined.string();
    if (comb_str.size() < root_str.size() || comb_str.compare(0, root_str.size(), root_str) != 0) {
        return false;
    }

    out_host_path = comb_str;
    return true;
}

int32_t HleLibKernel::sceKernelOpen(
    const std::string& guest_path,
    int32_t /*flags*/,
    int32_t /*mode*/)
{
    std::string host_path;
    if (!ResolveSandboxedGuestPath(guest_path, host_path)) {
        return SCE_KERNEL_ERROR_EACCES;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    int32_t fd = next_fd_++;
    open_files_[fd] = host_path;
    return fd;
}

int64_t HleLibKernel::sceKernelRead(int32_t fd, void* buf, size_t count) {
    if (!buf || count == 0) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    std::string host_path;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = open_files_.find(fd);
        if (it == open_files_.end()) {
            return SCE_KERNEL_ERROR_ENOENT;
        }
        host_path = it->second;
    }

    std::ifstream in(host_path, std::ios::binary);
    if (!in.is_open()) {
        return SCE_KERNEL_ERROR_ENOENT;
    }
    in.read(static_cast<char*>(buf), static_cast<std::streamsize>(count));
    return static_cast<int64_t>(in.gcount());
}

int32_t HleLibKernel::sceKernelClose(int32_t fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = open_files_.find(fd);
    if (it == open_files_.end()) {
        return SCE_KERNEL_ERROR_ENOENT;
    }
    open_files_.erase(it);
    return SCE_OK;
}

uint64_t HleLibKernel::sceKernelGetProcessTimeCounter() const {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

} // namespace ps5::emu
