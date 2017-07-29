
/*
================================================================================

  MIT License

  Copyright (c) 2017 Dynamic_Static

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

================================================================================
*/

#include "Dynamic_Static/Graphics/Vulkan/Buffer.hpp"
#include "Dynamic_Static/Graphics/Vulkan/Device.hpp"
#include "Dynamic_Static/Graphics/Vulkan/PhysicalDevice.hpp"
#include "Dynamic_Static/Graphics/Vulkan/Memory.hpp"

namespace Dynamic_Static {
namespace Graphics {
namespace Vulkan {

    Buffer::Buffer(const std::shared_ptr<Device>& device, const Info& info)
        : DeviceChild(device)
    {
        initialize(info);
    }

    Buffer::~Buffer()
    {
        if (mHandle) {
            mMemory.reset();
            vkDestroyBuffer(device(), mHandle, nullptr);
        }
    }

    VkMemoryRequirements Buffer::memory_requirements() const
    {
        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(device(), mHandle, &memoryRequirements);
        return memoryRequirements;
    }

    void Buffer::initialize(const Info& info)
    {
        validate(vkCreateBuffer(DeviceChild::device(), &info, nullptr, &mHandle));
        name("Dynamic_Static::Vulkan::Buffer");

        /*
        }
        */
            // TODO : Should this be moved to PhysicalDevice?
            // NOTE : We don't want to force these flags for every Buffer.
            auto memoryPropertyFlags =
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            auto memoryRequirements = memory_requirements();
            auto memoryTypeFilter = memoryRequirements.memoryTypeBits;
            auto memoryProperties = device().physical_device().memory_properties();
            int32_t memoryTypeIndex = -1;
            for (int32_t i = 0; i < static_cast<int32_t>(memoryProperties.memoryTypeCount); ++i) {
                if (memoryTypeFilter & (1 << i)) {
                    if ((memoryProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags) {
                        memoryTypeIndex = i;
                        break;
                    }
                }
            }

            if (memoryTypeIndex == -1) {
                throw std::runtime_error("Failed to find supported Memory type index");
            }
        /*
        }
        */

        // TODO : This is a lousy way to be using memory, we should be binding several
        //        related buffers to a single allocation and ensuring that our offsets
        //        are aligned to memoryRequirements.alignment.
        // TODO : Buffer shouldn't own a particular Memory allocation.
        //        http://gpuopen.com/vulkan-device-memory/
        //        https://twitter.com/axelgneiting/status/756218806570147840
        size_t offset = 0;
        Memory::Info memoryInfo;
        memoryInfo.memoryTypeIndex = static_cast<uint32_t>(memoryTypeIndex);
        memoryInfo.allocationSize = memoryRequirements.size;
        mMemory = device().allocate<Memory>(memoryInfo);
        validate(vkBindBufferMemory(device(), mHandle, *mMemory, static_cast<VkDeviceSize>(offset)));
    }

} // namespace Vulkan
} // namespace Graphics
} // namespace Dynamic_Static
