
/*
==========================================
  Copyright (c) 2016-2018 Dynamic_Static
    Patrick Purcell
      Licensed under the MIT license
    http://opensource.org/licenses/MIT
==========================================
*/

#include "Dynamic_Static/Graphics/Vulkan/Image.hpp"
#include "Dynamic_Static/Graphics/Vulkan/Device.hpp"

#include <utility>

namespace Dynamic_Static {
namespace Graphics {
namespace Vulkan {

    Image::Image(
        const std::shared_ptr<Device>& device,
        CreateInfo createInfo
    )
        : DeviceChild(device)
    {
        set_name("Image");
        dst_vk(vkCreateImage(get_device(), &createInfo, nullptr, &mHandle));
    }

    Image::Image(
        const std::shared_ptr<Device>& device,
        CreateInfo createInfo,
        VkImage handle
    )
        : DeviceChild(device)
    {
        set_name("Image");
        mHandle = handle;
    }

    Image::Image(Image&& other)
        : Object(std::move(other))
        , DeviceChild(std::move(other))
        , mCreateInfo { std::move(other.mCreateInfo) }
    {
    }

    Image::~Image()
    {
        if (mHandle) {
            vkDestroyImage(get_device(), mHandle, nullptr);
        }
    }

    Image& Image::operator=(Image&& other)
    {
        if (this != &other) {
            Object::operator=(std::move(other));
            DeviceChild::operator=(std::move(other));
            mCreateInfo = std::move(other.mCreateInfo);
        }
        return *this;
    }

    VkImageType Image::get_type() const
    {
        return mCreateInfo.imageType;
    }

    VkFormat Image::get_format() const
    {
        return mCreateInfo.format;
    }

    const VkExtent3D& Image::get_extent() const
    {
        return mCreateInfo.extent;
    }

    uint32_t Image::get_mip_level_count() const
    {
        return mCreateInfo.mipLevels;
    }

    uint32_t Image::get_array_layer_count() const
    {
        return mCreateInfo.arrayLayers;
    }

    VkSampleCountFlagBits Image::get_sample_count_flag() const
    {
        return mCreateInfo.samples;
    }

    VkImageTiling Image::get_tiling() const
    {
        return mCreateInfo.tiling;
    }

    VkImageUsageFlags Image::get_image_usage_flags() const
    {
        return mCreateInfo.usage;
    }

    VkSharingMode Image::get_sharing_mode() const
    {
        return mCreateInfo.sharingMode;
    }

} // namespace Vulkan
} // namespace Graphics
} // namespace Dynamic_Static
