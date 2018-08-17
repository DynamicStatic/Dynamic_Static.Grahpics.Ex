
/*
==========================================
  Copyright (c) 2016-2018 Dynamic_Static
    Patrick Purcell
      Licensed under the MIT license
    http://opensource.org/licenses/MIT
==========================================
*/

#pragma once

#include "Dynamic_Static/Graphics/Vulkan/Defines.hpp"
#include "Dynamic_Static/Graphics/Vulkan/DeviceChild.hpp"
#include "Dynamic_Static/Graphics/Vulkan/Object.hpp"

#include <memory>

namespace Dynamic_Static {
namespace Graphics {
namespace Vulkan {

    /*
    * Provides high level control over a Vulkan image.
    */
    class Image
        : public Object<VkImage>
        , public SharedObject<Image>
        , public DeviceChild
    {
    public:
        /*
        * Configuration parameters for Image creation.
        */
        struct CreateInfo final
            : public VkImageCreateInfo
        {
            /*
            * Constructs an instance of Image::CreateInfo.
            */
            CreateInfo()
            {
                sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                pNext = nullptr;
                flags = 0;
                imageType = VK_IMAGE_TYPE_1D;
                format = VK_FORMAT_UNDEFINED;
                extent = { 1, 1, 1 };
                mipLevels = 1;
                arrayLayers = 1;
                samples = VK_SAMPLE_COUNT_1_BIT;
                tiling = VK_IMAGE_TILING_OPTIMAL;
                usage = 0;
                sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                queueFamilyIndexCount = 0;
                pQueueFamilyIndices = nullptr;
                initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                static_assert(
                    sizeof(Image::CreateInfo) == sizeof(VkImageCreateInfo),
                    "sizeof(Image::CreateInfo) != sizeof(VkImageCreateInfo)"
                );
            }
        };

    private:
        CreateInfo mCreateInfo { };

    private:
        /*
        * Constructs an instance of Image.
        * @param [in] device This Image's Device
        * @param [in] createInfo This Image's Image::CreateInfo
        */
        Image(
            const std::shared_ptr<Device>& device,
            CreateInfo createInfo = { }
        );

        /*
        * Constructs an instance of Image.
        * @param [in] device This Image's Device
        * @param [in] createInfo This Image's Image::CreateInfo
        * @param [in] handle This Image's handle
        */
        Image(
            const std::shared_ptr<Device>& device,
            CreateInfo createInfo,
            VkImage handle
        );

    public:
        /*
        * Moves an instance of Image.
        * @param [in] other The Image to move from
        */
        Image(Image&& other);

        /*
        * Destroys this instance of Image.
        */
        ~Image();

        /*
        * Moves an instance of Image.
        * @param [in] other The Image to move from
        * @return This Image
        */
        Image& operator=(Image&& other);

    public:
        /*
        * Gets this Image's VkImageType.
        * @return This Image's VkImageType
        */
        VkImageType get_type() const;

        /*
        * Gets this Image's VkFormat.
        * @return This Image's VkFormat
        */
        VkFormat get_format() const;

        /*
        * Gets this Image's VkExtent3D.
        * @return This Image's VkExtent3D.
        */
        const VkExtent3D& get_extent() const;

        /*
        * Gets this Image's mip level count.
        * @return This Image's mip level count
        */
        uint32_t get_mip_level_count() const;

        /*
        * Gets this Image's array layer count.
        * @return This Image's array layer count
        */
        uint32_t get_array_layer_count() const;

        /*
        * Gets this Image's VkSampleCountFlagBits.
        * @return This Image's VkSampleCountFlagBits
        */
        VkSampleCountFlagBits get_sample_count_flag() const;

        /*
        * Gets this Image's VkImageTiling.
        * @return This Image's VkImageTiling
        */
        VkImageTiling get_tiling() const;

        /*
        * Gets this Image's VkImageUsageFlags.
        * @return This Image's VkImageUsageFlags
        */
        VkImageUsageFlags get_image_usage_flags() const;

        /*
        * Gets this Image's VkSharingMode.
        * @return This Image's VkSharingMode
        */
        VkSharingMode get_sharing_mode() const;

    private:
        friend class Device;
        friend class SwapchainKHR;
    };

} // namespace Vulkan
} // namespace Graphics
} // namespace Dynamic_Static
