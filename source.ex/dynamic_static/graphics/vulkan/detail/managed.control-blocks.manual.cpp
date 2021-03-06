
/*
==========================================
    Copyright (c) 2020 Dynamic_Static
    Patrick Purcell
        Licensed under the MIT license
    http://opensource.org/licenses/MIT
==========================================
*/

#include "dynamic_static/graphics/vulkan/detail/managed.control-blocks.manual.hpp"
#include "dynamic_static/graphics/vulkan/generated/managed.control-blocks.hpp"
#include "dynamic_static/graphics/vulkan/managed.hpp"

#include <algorithm>

namespace dst {
namespace vk {
namespace detail {

template <>
void on_managed_handle_created(Managed<VkInstance>& instance)
{
    // TODO : Scratch pad allocator...
    // TODO : Error handling...
    uint32_t physicalDeviceCount = 0;
    dst_vk(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr));
    std::vector<VkPhysicalDevice> vkPhysicalDevices(physicalDeviceCount);
    dst_vk(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, vkPhysicalDevices.data()));
    std::vector<Managed<VkPhysicalDevice>> physicalDevices(physicalDeviceCount);
    for (uint32_t i = 0; i < physicalDeviceCount; ++i) {
        auto& physicalDevice = physicalDevices[i];
        physicalDevice.mVkHandle = vkPhysicalDevices[i];
        physicalDevice.mspControlBlock = Managed<VkPhysicalDevice>::create_control_block(vkPhysicalDevices[i]);
        physicalDevice.mspControlBlock->set(VK_OBJECT_TYPE_PHYSICAL_DEVICE);
        physicalDevice.mspControlBlock->set(std::move(Managed<VkInstance>(instance)));
        physicalDevice.mspControlBlock->set(std::move(vkPhysicalDevices[i]));
    }
    instance.mspControlBlock->set(std::move(physicalDevices));
}

template <>
void on_managed_handle_destroyed(Managed<VkInstance>& instance)
{
    if (instance) {
        assert(instance.mspControlBlock);
        resolve_circular_references<Managed<VkInstance>, Managed<VkPhysicalDevice>>(instance);
    }
}

template <>
void on_managed_handle_destroyed(Managed<VkPhysicalDevice>& physicalDevice)
{
    if (physicalDevice) {
        assert(physicalDevice.mspControlBlock);
        resolve_circular_references<Managed<VkInstance>, Managed<VkPhysicalDevice>>(physicalDevice.get<Managed<VkInstance>>());
    }
}

template <>
void on_managed_handle_created(Managed<VkDevice>& device)
{
    // TODO : Scratch pad allocator...
    const auto& deviceCreateInfo = device.get<Managed<VkDeviceCreateInfo>>();
    if (deviceCreateInfo->queueCreateInfoCount && deviceCreateInfo->pQueueCreateInfos) {
        std::vector<Managed<VkQueue>> queues;
        queues.reserve(deviceCreateInfo->queueCreateInfoCount);
        for (uint32_t queueCreateInfo_i = 0; queueCreateInfo_i < deviceCreateInfo->queueCreateInfoCount; ++queueCreateInfo_i) {
            const auto& deviceQueueCreateInfo = deviceCreateInfo->pQueueCreateInfos[queueCreateInfo_i];
            for (uint32_t queue_i = 0; queue_i < deviceQueueCreateInfo.queueCount; ++queue_i) {
                VkQueue vkQueue = VK_NULL_HANDLE;
                vkGetDeviceQueue(device, deviceQueueCreateInfo.queueFamilyIndex, queue_i, &vkQueue);
                assert(vkQueue);
                Managed<VkQueue> queue;
                queue.mVkHandle = vkQueue;
                queue.mspControlBlock = Managed<VkQueue>::create_control_block(vkQueue);
                queue.mspControlBlock->set(VK_OBJECT_TYPE_QUEUE);
                queue.mspControlBlock->set(std::move(Managed<VkDevice>(device)));
                queue.mspControlBlock->set(std::move(Managed<VkDeviceQueueCreateInfo>(deviceQueueCreateInfo)));
                queue.mspControlBlock->set(std::move(vkQueue));
                queues.push_back(std::move(queue));
            }
        }
        device.mspControlBlock->set(std::move(queues));
    }
}

template <>
void on_managed_handle_destroyed(Managed<VkDevice>& device)
{
    if (device) {
        assert(device.mspControlBlock);
        resolve_circular_references<Managed<VkDevice>, Managed<VkQueue>>(device);
    }
}

template <>
void on_managed_handle_destroyed(Managed<VkQueue>& queue)
{
    if (queue) {
        assert(queue.mspControlBlock);
        resolve_circular_references<Managed<VkDevice>, Managed<VkQueue>>(queue.get<Managed<VkDevice>>());
    }
}

template <>
void on_managed_handle_created(Managed<VkSwapchainKHR>& swapchain)
{
    // TODO : Scratch pad allocator...
    // TODO : Error handling...
    assert(swapchain);
    const auto& device = swapchain.get<Managed<VkDevice>>();
    assert(device);
    uint32_t imageCount = 0;
    dst_vk(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr));
    assert(imageCount);
    std::vector<VkImage> vkImages(imageCount);
    dst_vk(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, vkImages.data()));
    std::vector<Managed<VkImage>> images(vkImages.size());
    const auto& swapchainCreateInfo = swapchain.get<Managed<VkSwapchainCreateInfoKHR>>();
    VkImageCreateInfo imageCreateInfo { };
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = swapchainCreateInfo->imageFormat;
    imageCreateInfo.extent.width = swapchainCreateInfo->imageExtent.width;
    imageCreateInfo.extent.height = swapchainCreateInfo->imageExtent.height;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = swapchainCreateInfo->imageArrayLayers;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = swapchainCreateInfo->imageUsage;
    imageCreateInfo.sharingMode = swapchainCreateInfo->imageSharingMode;
    imageCreateInfo.queueFamilyIndexCount = swapchainCreateInfo->queueFamilyIndexCount;
    imageCreateInfo.pQueueFamilyIndices = swapchainCreateInfo->pQueueFamilyIndices;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    for (uint32_t i = 0; i < imageCount; ++i) {
        auto& image = images[i];
        image.mVkHandle = vkImages[i];
        image.mspControlBlock = Managed<VkImage>::create_control_block(vkImages[i]);
        image.mspControlBlock->set(VK_OBJECT_TYPE_IMAGE);
        image.mspControlBlock->set(std::move(Managed<VkDevice>(device)));
        image.mspControlBlock->set(std::move(Managed<VkImageCreateInfo>(imageCreateInfo)));
        image.mspControlBlock->set(std::move(Managed<VkSwapchainKHR>(swapchain)));
        image.mspControlBlock->set(std::move(vkImages[i]));
    }
    swapchain.mspControlBlock->set(std::move(images));
}

template <>
void on_managed_handle_destroyed(Managed<VkSwapchainKHR>& swapchain)
{
    if (swapchain) {
        assert(swapchain.mspControlBlock);
        resolve_circular_references<Managed<VkSwapchainKHR>, Managed<VkImage>>(swapchain);
    }
}

template <>
void on_managed_handle_destroyed(Managed<VkImage>& image)
{
    if (image) {
        assert(image.mspControlBlock);
        resolve_circular_references<Managed<VkSwapchainKHR>, Managed<VkImage>>(image.get<Managed<VkSwapchainKHR>>());
    }
}

template <>
VkResult bind_memory(Managed<VkBuffer>& buffer, Managed<VkDeviceMemory>& memory, VkDeviceSize memoryOffset)
{
    auto vkResult = VK_ERROR_INITIALIZATION_FAILED;
    if (buffer && memory) {
        assert(buffer.mspControlBlock);
        assert(buffer.get<Managed<VkDevice>>());
        vkResult = dst_vk(vkBindBufferMemory(buffer.get<Managed<VkDevice>>(), buffer, memory, memoryOffset));
        if (vkResult == VK_SUCCESS) {
            buffer.mspControlBlock->set(std::move(Managed<VkDeviceMemory>(memory)));
            auto bindBufferMemoryInfo = get_default<VkBindBufferMemoryInfo>();
            bindBufferMemoryInfo.buffer = buffer;
            bindBufferMemoryInfo.memory = memory;
            bindBufferMemoryInfo.memoryOffset = memoryOffset;
            buffer.mspControlBlock->set(std::move(Managed<VkBindBufferMemoryInfo>(bindBufferMemoryInfo)));
        }
    }
    return vkResult;
}

template <>
VkResult bind_memory(Managed<VkImage>& image, Managed<VkDeviceMemory>& memory, VkDeviceSize memoryOffset)
{
    auto vkResult = VK_ERROR_INITIALIZATION_FAILED;
    if (image && memory) {
        assert(image.mspControlBlock);
        assert(image.get<Managed<VkDevice>>());
        vkResult = dst_vk(vkBindImageMemory(image.get<Managed<VkDevice>>(), image, memory, memoryOffset));
        if (vkResult == VK_SUCCESS) {
            image.mspControlBlock->set(std::move(Managed<VkDeviceMemory>(memory)));
            auto bindImageMemoryInfo = get_default<VkBindImageMemoryInfo>();
            bindImageMemoryInfo.image = image;
            bindImageMemoryInfo.memory = memory;
            bindImageMemoryInfo.memoryOffset = memoryOffset;
            image.mspControlBlock->set(std::move(Managed<VkBindImageMemoryInfo>(bindImageMemoryInfo)));
        }
    }
    return vkResult;
}

template <>
VkResult bind_memory(VkDevice vkDevice, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos)
{
    auto vkResult = dst_vk(vkBindBufferMemory2(vkDevice, bindInfoCount, pBindInfos));
    if (vkResult == VK_SUCCESS && bindInfoCount && pBindInfos) {
        for (uint32_t i = 0; i < bindInfoCount; ++i) {
            Managed<VkBuffer> buffer(pBindInfos[i].buffer);
            if (buffer) {
                assert(buffer.get<Managed<VkDevice>>() == vkDevice && "TODO : Better error handling");
                buffer.mspControlBlock->set(std::move(Managed<VkDeviceMemory>(pBindInfos[i].memory)));
                buffer.mspControlBlock->set(std::move(Managed<VkBindBufferMemoryInfo>(pBindInfos[i])));
            }
        }
    }
    return vkResult;
}

template <>
VkResult bind_memory(VkDevice vkDevice, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos)
{
    auto vkResult = dst_vk(vkBindImageMemory2(vkDevice, bindInfoCount, pBindInfos));
    if (vkResult == VK_SUCCESS && bindInfoCount && pBindInfos) {
        for (uint32_t i = 0; i < bindInfoCount; ++i) {
            Managed<VkImage> image(pBindInfos[i].image);
            if (image) {
                assert(image.get<Managed<VkDevice>>() == vkDevice && "TODO : Better error handling");
                image.mspControlBlock->set(std::move(Managed<VkDeviceMemory>(pBindInfos[i].memory)));
                image.mspControlBlock->set(std::move(Managed<VkBindImageMemoryInfo>(pBindInfos[i])));
            }
        }
    }
    return vkResult;
}

} // namespace detail

Managed<VkBufferView>::ControlBlock::ControlBlock(VkBufferView vkHandle)
{
    assert(vkHandle);
    set(std::move(vkHandle));
}

VkResult Managed<VkBufferView>::ControlBlock::create(const Managed<VkDevice>& device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, Managed<VkBufferView>* pView)
{
    auto vkResult = VK_ERROR_INITIALIZATION_FAILED;
    if (pView) {
        pView->reset();
        Managed<VkBuffer> buffer(pCreateInfo ? pCreateInfo->buffer : VK_NULL_HANDLE);
        if (buffer) {
            VkBufferView vkHandle = VK_NULL_HANDLE;
            vkResult = vkCreateBufferView(device, pCreateInfo, pAllocator, &vkHandle);
            if (vkResult == VK_SUCCESS) {
                pView->mVkHandle = vkHandle;
                pView->mspControlBlock = create_control_block(vkHandle);
                pView->mspControlBlock->set(VK_OBJECT_TYPE_BUFFER_VIEW);
                pView->mspControlBlock->set(std::move(Managed<VkDevice>(device)));
                pView->mspControlBlock->set(std::move(Managed<VkBufferViewCreateInfo>(*pCreateInfo)));
                pView->mspControlBlock->set(std::move(pAllocator ? *pAllocator : VkAllocationCallbacks { }));
                pView->mspControlBlock->set(std::move(buffer));
                pView->mspControlBlock->set(std::move(vkHandle));
                detail::on_managed_handle_created(*pView);
            }
        }
    }
    return vkResult;
}

Managed<VkBufferView>::ControlBlock::~ControlBlock()
{
    assert(get<VkBufferView>());
    unregister_control_block(get<VkBufferView>());
    if (get<VkObjectType>() != VK_OBJECT_TYPE_MAX_ENUM) {
        auto vkDevice = *get<Managed<VkDevice>>();
        assert(vkDevice);
        vkDestroyBufferView(vkDevice, get<VkBufferView>(), get<VkAllocationCallbacks>().pfnFree ? &get<VkAllocationCallbacks>() : nullptr);
    }
}

Managed<VkCommandBuffer>::ControlBlock::ControlBlock(VkCommandBuffer vkHandle)
{
    assert(vkHandle);
    set(std::move(vkHandle));
}

VkResult Managed<VkCommandBuffer>::ControlBlock::allocate(const Managed<VkDevice>& device, const VkCommandBufferAllocateInfo* pAllocateInfo, Managed<VkCommandBuffer>* pCommandBuffers)
{
    auto vkResult = VK_ERROR_INITIALIZATION_FAILED;
    if (pAllocateInfo && pAllocateInfo->commandBufferCount && pCommandBuffers) {
        for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; ++i) {
            pCommandBuffers[i].reset();
        }
        Managed<VkCommandPool> commandPool(pAllocateInfo->commandPool);
        if (commandPool) {
            // TODO : Scratch pad allocator...
            std::vector<VkCommandBuffer> vkCommandBuffers(pAllocateInfo->commandBufferCount);
            vkResult = dst_vk(vkAllocateCommandBuffers(device, pAllocateInfo, vkCommandBuffers.data()));
            if (vkResult == VK_SUCCESS) {
                for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; ++i) {
                    pCommandBuffers[i].mVkHandle = vkCommandBuffers[i];
                    pCommandBuffers[i].mspControlBlock = Managed<VkCommandBuffer>::create_control_block(vkCommandBuffers[i]);
                    pCommandBuffers[i].mspControlBlock->set(VK_OBJECT_TYPE_COMMAND_BUFFER);
                    pCommandBuffers[i].mspControlBlock->set(std::move(Managed<VkCommandPool>(commandPool)));
                    pCommandBuffers[i].mspControlBlock->set(std::move(Managed<VkCommandBufferAllocateInfo>(*pAllocateInfo)));
                    pCommandBuffers[i].mspControlBlock->set(std::move(vkCommandBuffers[i]));
                }
            }
        }
    }
    return vkResult;
}

Managed<VkCommandBuffer>::ControlBlock::~ControlBlock()
{
    assert(get<VkCommandBuffer>());
    unregister_control_block(get<VkCommandBuffer>());
    const auto& commandPool = get<Managed<VkCommandPool>>();
    assert(commandPool);
    const auto& device = commandPool.get<Managed<VkDevice>>();
    assert(device);
    vkFreeCommandBuffers(device, commandPool, 1, &get<VkCommandBuffer>());
}

Managed<VkDescriptorSet>::ControlBlock::ControlBlock(VkDescriptorSet vkHandle)
{
    assert(vkHandle);
    set(std::move(vkHandle));
}

VkResult Managed<VkDescriptorSet>::ControlBlock::allocate(const Managed<VkDevice>& device, const VkDescriptorSetAllocateInfo* pAllocateInfo, Managed<VkDescriptorSet>* pDescriptorSets)
{
    auto vkResult = VK_ERROR_INITIALIZATION_FAILED;
    if (pAllocateInfo && pAllocateInfo->descriptorSetCount && pDescriptorSets) {
        for (uint32_t i = 0; i < pAllocateInfo->descriptorSetCount; ++i) {
            pDescriptorSets[i].reset();
        }
        Managed<VkDescriptorPool> descriptorPool(pAllocateInfo->descriptorPool);
        if (descriptorPool) {
            // TODO : Scratch pad allocator...
            std::vector<VkDescriptorSet> vkDescriptorSets(pAllocateInfo->descriptorSetCount);
            vkResult = dst_vk(vkAllocateDescriptorSets(device, pAllocateInfo, vkDescriptorSets.data()));
            if (vkResult == VK_SUCCESS) {
                for (uint32_t i = 0; i < pAllocateInfo->descriptorSetCount; ++i) {
                    pDescriptorSets[i].mVkHandle = vkDescriptorSets[i];
                    pDescriptorSets[i].mspControlBlock = Managed<VkDescriptorSet>::create_control_block(vkDescriptorSets[i]);
                    pDescriptorSets[i].mspControlBlock->set(VK_OBJECT_TYPE_COMMAND_BUFFER);
                    pDescriptorSets[i].mspControlBlock->set(std::move(Managed<VkDescriptorPool>(descriptorPool)));
                    pDescriptorSets[i].mspControlBlock->set(std::move(Managed<VkDescriptorSetAllocateInfo>(*pAllocateInfo)));
                    pDescriptorSets[i].mspControlBlock->set(std::move(vkDescriptorSets[i]));
                }
            }
        }
    }
    return vkResult;
}

Managed<VkDescriptorSet>::ControlBlock::~ControlBlock()
{
    assert(get<VkDescriptorSet>());
    unregister_control_block(get<VkDescriptorSet>());
    const auto& descriptorPool = get<Managed<VkDescriptorPool>>();
    assert(descriptorPool);
    const auto& device = descriptorPool.get<Managed<VkDevice>>();
    assert(device);
    vkFreeDescriptorSets(device, descriptorPool, 1, &get<VkDescriptorSet>());
}

Managed<VkFramebuffer>::ControlBlock::ControlBlock(VkFramebuffer vkHandle)
{
    assert(vkHandle);
    set(std::move(vkHandle));
}

VkResult Managed<VkFramebuffer>::ControlBlock::create(const Managed<VkDevice>& device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, Managed<VkFramebuffer>* pFramebuffer)
{
    auto vkResult = VK_ERROR_INITIALIZATION_FAILED;
    if (pFramebuffer) {
        pFramebuffer->reset();
        // TODO : Scratch pad allocator...
        if (pCreateInfo && pCreateInfo->attachmentCount && pCreateInfo->pAttachments) {
            std::vector<Managed<VkImageView>> attachments(pCreateInfo->attachmentCount);
            for (uint32_t i = 0; i < pCreateInfo->attachmentCount; ++i) {
                attachments[i] = pCreateInfo->pAttachments[i];
                if (!attachments[i]) {
                    return vkResult;
                }
            }
            VkFramebuffer vkHandle = VK_NULL_HANDLE;
            vkResult = vkCreateFramebuffer(device, pCreateInfo, pAllocator, &vkHandle);
            if (vkResult == VK_SUCCESS) {
                pFramebuffer->mVkHandle = vkHandle;
                pFramebuffer->mspControlBlock = create_control_block(vkHandle);
                pFramebuffer->mspControlBlock->set(VK_OBJECT_TYPE_FRAMEBUFFER);
                pFramebuffer->mspControlBlock->set(std::move(Managed<VkDevice>(device)));
                pFramebuffer->mspControlBlock->set(std::move(Managed<VkFramebufferCreateInfo>(*pCreateInfo)));
                pFramebuffer->mspControlBlock->set(std::move(pAllocator ? *pAllocator : VkAllocationCallbacks { }));
                pFramebuffer->mspControlBlock->set(std::move(attachments));
                pFramebuffer->mspControlBlock->set(std::move(vkHandle));
                detail::on_managed_handle_created(*pFramebuffer);
            }
        }
    }
    return vkResult;
}

Managed<VkFramebuffer>::ControlBlock::~ControlBlock()
{
    assert(get<VkFramebuffer>());
    unregister_control_block(get<VkFramebuffer>());
    if (get<VkObjectType>() != VK_OBJECT_TYPE_MAX_ENUM) {
        auto vkDevice = *get<Managed<VkDevice>>();
        assert(vkDevice);
        vkDestroyFramebuffer(vkDevice, get<VkFramebuffer>(), get<VkAllocationCallbacks>().pfnFree ? &get<VkAllocationCallbacks>() : nullptr);
    }
}

Managed<VkImageView>::ControlBlock::ControlBlock(VkImageView vkHandle)
{
    assert(vkHandle);
    set(std::move(vkHandle));
}

VkResult Managed<VkImageView>::ControlBlock::create(const Managed<VkDevice>& device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, Managed<VkImageView>* pView)
{
    auto vkResult = VK_ERROR_INITIALIZATION_FAILED;
    if (pView) {
        pView->reset();
        Managed<VkImage> image(pCreateInfo ? pCreateInfo->image : VK_NULL_HANDLE);
        if (image) {
            VkImageView vkHandle = VK_NULL_HANDLE;
            vkResult = vkCreateImageView(device, pCreateInfo, pAllocator, &vkHandle);
            if (vkResult == VK_SUCCESS) {
                pView->mVkHandle = vkHandle;
                pView->mspControlBlock = create_control_block(vkHandle);
                pView->mspControlBlock->set(VK_OBJECT_TYPE_IMAGE_VIEW);
                pView->mspControlBlock->set(std::move(Managed<VkDevice>(device)));
                pView->mspControlBlock->set(std::move(Managed<VkImageViewCreateInfo>(*pCreateInfo)));
                pView->mspControlBlock->set(std::move(pAllocator ? *pAllocator : VkAllocationCallbacks { }));
                pView->mspControlBlock->set(std::move(image));
                pView->mspControlBlock->set(std::move(vkHandle));
                detail::on_managed_handle_created(*pView);
            }
        }
    }
    return vkResult;
}

#if 0
VkResult Managed<VkImageView>::ControlBlock::create(const Managed<VkImage>& image, const VkAllocationCallbacks* pAllocator, Managed<VkImageView>* pView)
{
    // TODO : This function needs some work to support VK_IMAGE_VIEW_TYPE_CUBE,
    //  and VK_IMAGE_VIEW_TYPE_CUBE_ARRAY...there should also be checks that the
    //  VkImage has proper flags set for VK_IMAGE_VIEW_TYPE_2D_ARRAY
    auto vkResult = VK_ERROR_INITIALIZATION_FAILED;
    if (pView) {
        pView->reset();
        if (image) {
            auto const& imageCreateInfo = image.get<VkImageCreateInfo>();
            auto imageViewCreateInfo = get_default<VkImageViewCreateInfo>();
            imageViewCreateInfo.image = image;
            if (1 <= imageCreateInfo.arrayLayers) {
                switch (imageCreateInfo.imageType) {
                case VK_IMAGE_TYPE_1D: imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_1D; break;
                case VK_IMAGE_TYPE_2D: imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_1D; break;
                case VK_IMAGE_TYPE_3D: imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_3D; break;
                default: return VK_ERROR_VALIDATION_FAILED_EXT;
                }
            } else {
                switch (imageCreateInfo.imageType) {
                case VK_IMAGE_TYPE_1D: imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY; break;
                case VK_IMAGE_TYPE_2D: imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY; break;
                default: return VK_ERROR_VALIDATION_FAILED_EXT;
                }
            }
            imageViewCreateInfo.format = imageCreateInfo.format;
            if (imageCreateInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            } else {
                imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            }
            imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
            imageViewCreateInfo.subresourceRange.levelCount = imageCreateInfo.mipLevels;
            imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
            imageViewCreateInfo.subresourceRange.layerCount = imageCreateInfo.arrayLayers;
            vkResult = vk::create<Managed<VkImageView>>(image.get<Managed<VkDevice>>(), &imageViewCreateInfo, pAllocator, pView);
        }
    }
    return vkResult;
}
#endif

Managed<VkImageView>::ControlBlock::~ControlBlock()
{
    assert(get<VkImageView>());
    unregister_control_block(get<VkImageView>());
    if (get<VkObjectType>() != VK_OBJECT_TYPE_MAX_ENUM) {
        auto vkDevice = *get<Managed<VkDevice>>();
        assert(vkDevice);
        vkDestroyImageView(vkDevice, get<VkImageView>(), get<VkAllocationCallbacks>().pfnFree ? &get<VkAllocationCallbacks>() : nullptr);
    }
}

} // namespace vk
} // namespace dst
