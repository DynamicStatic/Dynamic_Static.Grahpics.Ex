
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

// Based on "Make a Neon Vector Shooter in XNA"
// https://gamedevelopment.tutsplus.com/series/cross-platform-vector-shooter-xna--gamedev-10559

#include "Resources.hpp"

#include "Dynamic_Static/Core/Math.hpp"
#include "Dynamic_Static/Core/Time.hpp"
#include "Dynamic_Static/Graphics/ImageCache.hpp"
#include "Dynamic_Static/Graphics/ImageReader.hpp"
#include "Dynamic_Static/Graphics/Vulkan.hpp"
#include "Dynamic_Static/Graphics/Window.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <memory>

using namespace dst::gfx;
using namespace dst::gfx::vlkn;

struct UniformBuffer final
{
    dst::Matrix4x4 world;
    dst::Matrix4x4 view;
    dst::Matrix4x4 projection;
};

// struct Vertex final
// {
//     dst::Vector3 position;
//     dst::Vector2 texCoord;
//     dst::Color color;
// 
//     static VkVertexInputBindingDescription binding_description()
//     {
//         VkVertexInputBindingDescription binding;
//         binding.binding = 0;
//         binding.stride = sizeof(Vertex);
//         binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
//         return binding;
//     }
// 
//     static std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions()
//     {
//         VkVertexInputAttributeDescription positionAttribute;
//         positionAttribute.binding = 0;
//         positionAttribute.location = 0;
//         positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
//         positionAttribute.offset = offsetof(Vertex, position);
// 
//         VkVertexInputAttributeDescription texCoordAttribute;
//         texCoordAttribute.binding = 0;
//         texCoordAttribute.location = 1;
//         texCoordAttribute.format = VK_FORMAT_R32G32_SFLOAT;
//         texCoordAttribute.offset = offsetof(Vertex, texCoord);
// 
//         VkVertexInputAttributeDescription colorAttribute;
//         colorAttribute.binding = 0;
//         colorAttribute.location = 2;
//         colorAttribute.format = VK_FORMAT_R32G32B32A32_SFLOAT;
//         colorAttribute.offset = offsetof(Vertex, color);
// 
//         return {
//             positionAttribute,
//             texCoordAttribute,
//             colorAttribute
//         };
//     }
// };

template <typename FuncType>
void process_transient_command_buffer(Command::Pool& commandPool, Queue& queue, const FuncType& f)
{
    auto commandBuffer = commandPool.allocate_transient<Command::Buffer>();
    Command::Buffer::BeginInfo beginInfo;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    commandBuffer->begin(beginInfo);
    f(*commandBuffer);
    commandBuffer->end();
    Queue::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer->handle();
    queue.submit(submitInfo);
    queue.wait_idle();
}

VkImageMemoryBarrier create_layout_transition_barrier(Image& image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkImageMemoryBarrier barrier { };
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.image = image;

    if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    } else
    if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else
    if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    } else
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    } else {
        throw std::invalid_argument("Unsupported layout transition");
    }

    return barrier;
}

int main()
{
    {
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Create Instance and PhysicalDevices
        std::vector<std::string> instanceLayers;
        std::vector<std::string> instanceExtensions {
            VK_KHR_SURFACE_EXTENSION_NAME,
            #if defined(DYNAMIC_STATIC_WINDOWS)
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
            #elif defined(DYNAMIC_STATIC_LINUX)
            VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
            #endif
        };

        VkDebugReportFlagsEXT debugFlags =
            0
            #if defined(DYNAMIC_STATIC_WINDOWS)
            | VK_DEBUG_REPORT_INFORMATION_BIT_EXT
            | VK_DEBUG_REPORT_DEBUG_BIT_EXT
            | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
            | VK_DEBUG_REPORT_WARNING_BIT_EXT
            | VK_DEBUG_REPORT_ERROR_BIT_EXT
            #endif
            ;

        auto instance = vlkn::create<Instance>(instanceLayers, instanceExtensions, debugFlags);
        // NOTE : We're just assuming that the first PhysicalDevice is the one we want.
        //        This won't always be the case, we should check for necessary features.
        auto& physicalDevice = *instance->physical_devices()[0];
        auto apiVersion = physicalDevice.properties().apiVersion;

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Create Window and Surface
        Window::Configuration configuration;
        configuration.api = dst::gfx::API::Vulkan;
        configuration.apiVersion.major = VK_VERSION_MAJOR(apiVersion);
        configuration.apiVersion.minor = VK_VERSION_MAJOR(apiVersion);
        configuration.apiVersion.patch = VK_VERSION_MAJOR(apiVersion);
        auto window = std::make_shared<Window>(configuration);
        auto surface = physicalDevice.create<SurfaceKHR>(window);

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Create logical Device and Queues
        std::vector<std::string> deviceLayers;
        std::vector<std::string> deviceExtensions {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };

        auto queueFamilyFlags = VK_QUEUE_GRAPHICS_BIT;
        auto queueFamilyIndices = physicalDevice.find_queue_families(queueFamilyFlags);
        Queue::Info queueInfo { };
        std::array<float, 1> queuePriorities { };
        queueInfo.pQueuePriorities = queuePriorities.data();
        // NOTE : We're assuming that we got at least one Queue capabale of
        //        graphics, in anything but a toy we want to validate that.
        queueInfo.queueFamilyIndex = static_cast<uint32_t>(queueFamilyIndices[0]);
        std::array<Queue::Info, 1> queueInfos { queueInfo };
        auto device = physicalDevice.create<Device>(deviceLayers, deviceExtensions, queueInfos);
        // NOTE : We're assuming that the Queue we're choosing for graphics
        //        is capable of presenting, this may not always be the case.
        auto& graphicsQueue = *device->queues()[0][0];
        auto& presentQueue = graphicsQueue;

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Create SwapChain
        std::shared_ptr<SwapchainKHR> swapchain;
        if (surface->presentation_supported(presentQueue.family_index())) {
            swapchain = device->create<SwapchainKHR>(surface);
        } else {
            throw std::runtime_error("Surface doesn't support presentation");
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Create RenderPass

        // VkAttachmentDescription colorAttachment { };
        // colorAttachment.format = swapchain->format();
        // colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        // colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        // colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        // colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        // colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        // VkAttachmentReference colorAttachmentReference { };
        // colorAttachmentReference.attachment = 0;
        // colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        // 
        // std::array<VkFormat, 3> depthFormats {
        //     VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT
        // };
        // 
        // auto depthFormat = physicalDevice.find_supported_format(
        //     depthFormats,
        //     VK_IMAGE_TILING_OPTIMAL,
        //     VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        // );
        // 
        // VkAttachmentDescription depthAttachment { };
        // depthAttachment.format = depthFormat;
        // depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        // depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        // depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        // depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        // VkAttachmentReference depthAttachmentReference { };
        // depthAttachmentReference.attachment = 1;
        // depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        // 
        // VkSubpassDescription subpass { };
        // subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        // subpass.colorAttachmentCount = 1;
        // subpass.pColorAttachments = &colorAttachmentReference;
        // subpass.pDepthStencilAttachment = &depthAttachmentReference;
        // 
        // std::array<VkAttachmentDescription, 2> attachments { colorAttachment, depthAttachment };
        // RenderPass::Info renderPassInfo;
        // renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        // renderPassInfo.pAttachments = attachments.data();
        // renderPassInfo.subpassCount = 1;
        // renderPassInfo.pSubpasses = &subpass;
        // auto renderPass = device->create<RenderPass>(renderPassInfo);

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Create DescriptorSet::Layout

        // VkDescriptorSetLayoutBinding uniformBufferLayoutBinding { };
        // uniformBufferLayoutBinding.binding = 0;
        // uniformBufferLayoutBinding.descriptorCount = 1;
        // uniformBufferLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        // uniformBufferLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        // 
        // VkDescriptorSetLayoutBinding samplerLayoutBinding { };
        // samplerLayoutBinding.binding = 1;
        // samplerLayoutBinding.descriptorCount = 1;
        // samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        // samplerLayoutBinding.pImmutableSamplers = nullptr;
        // samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        // 
        // Descriptor::Set::Layout::Info descriptorSetLayoutInfo;
        // std::array<VkDescriptorSetLayoutBinding, 2> descriptorSetLayoutBindings {
        //     uniformBufferLayoutBinding,
        //     samplerLayoutBinding,
        // };
        // 
        // descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
        // descriptorSetLayoutInfo.pBindings = descriptorSetLayoutBindings.data();
        // auto descriptorSetLayout = device->create<Descriptor::Set::Layout>(descriptorSetLayoutInfo);

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Create Pipeline

        // auto vertexShader = device->create<ShaderModule>(
        //     VK_SHADER_STAGE_VERTEX_BIT,
        //     ShaderModule::Source::Code,
        //     R"(
        // 
        //         #version 450
        //         #extension GL_ARB_separate_shader_objects : enable
        // 
        //         layout(binding = 0)
        //         uniform UniformBuffer
        //         {
        //             mat4 world;
        //             mat4 view;
        //             mat4 projection;
        //         } ubo;
        // 
        //         layout(location = 0) in vec3 inPosition;
        //         layout(location = 1) in vec2 inTexCoord;
        //         // layout(location = 2) in vec4 inColor;
        // 
        //         layout(location = 0) out vec2 fragTexCoord;
        //         // layout(location = 1) out vec4 fragColor;
        // 
        //         out gl_PerVertex
        //         {
        //             vec4 gl_Position;
        //         };
        // 
        //         void main()
        //         {
        //             gl_Position = ubo.projection * ubo.view * ubo.world * vec4(inPosition, 1);
        //             fragTexCoord = inTexCoord;
        //             // fragColor = inColor;
        //         }
        // 
        //     )"
        // );
        // 
        // auto fragmentShader = device->create<ShaderModule>(
        //     VK_SHADER_STAGE_FRAGMENT_BIT,
        //     ShaderModule::Source::Code,
        //     R"(
        // 
        //         #version 450
        //         #extension GL_ARB_separate_shader_objects : enable
        // 
        //         layout(binding = 1) uniform sampler2D imageSampler;
        // 
        //         layout(location = 0) in vec2 fragTexCoord;
        //         // layout(location = 1) in vec4 fragColor;
        // 
        //         layout(location = 0) out vec4 outColor;
        // 
        //         void main()
        //         {
        //             outColor = texture(imageSampler, fragTexCoord);
        //         }
        // 
        //     )"
        // );
        // 
        // std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages {
        //     vertexShader->pipeline_stage_info(),
        //     fragmentShader->pipeline_stage_info(),
        // };
        // 
        // // auto vertexBindingDescription = Vertex::binding_description();
        // // auto vertexAttributeDescriptions = Vertex::attribute_descriptions();
        // auto vertexBindingDescription = ShapeBlaster::QuadVertex::binding_description();
        // auto vertexAttributeDescriptions = ShapeBlaster::QuadVertex::attribute_descriptions();
        // VkPipelineVertexInputStateCreateInfo vertexInputInfo { };
        // vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        // vertexInputInfo.vertexBindingDescriptionCount = 1;
        // vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDescription;
        // vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size());
        // vertexInputInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions.data();
        // 
        // VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo { };
        // inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        // inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        // 
        // VkViewport viewport { };
        // viewport.width = static_cast<float>(swapchain->extent().width);
        // viewport.height = static_cast<float>(swapchain->extent().height);
        // viewport.minDepth = 0;
        // viewport.maxDepth = 1;
        // 
        // VkRect2D scissor { };
        // scissor.extent = swapchain->extent();
        // 
        // VkPipelineViewportStateCreateInfo viewportInfo { };
        // viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        // viewportInfo.viewportCount = 1;
        // viewportInfo.pViewports = &viewport;
        // viewportInfo.scissorCount = 1;
        // viewportInfo.pScissors = &scissor;
        // 
        // VkPipelineRasterizationStateCreateInfo rasterizationInfo { };
        // rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        // rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
        // rasterizationInfo.lineWidth = 1;
        // rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        // rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        // 
        // VkPipelineMultisampleStateCreateInfo multisampleInfo { };
        // multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        // multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        // multisampleInfo.minSampleShading = 1;
        // 
        // VkPipelineDepthStencilStateCreateInfo depthStencilInfo { };
        // depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        // depthStencilInfo.depthTestEnable = VK_TRUE;
        // depthStencilInfo.depthWriteEnable = VK_TRUE;
        // depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
        // depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
        // depthStencilInfo.stencilTestEnable = VK_FALSE;
        // 
        // VkPipelineColorBlendAttachmentState colorBlendAttachment { };
        // colorBlendAttachment.blendEnable = VK_TRUE;
        // colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        // colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        // colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        // colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        // colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        // colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        // 
        // colorBlendAttachment.colorWriteMask =
        //     VK_COLOR_COMPONENT_R_BIT |
        //     VK_COLOR_COMPONENT_G_BIT |
        //     VK_COLOR_COMPONENT_B_BIT |
        //     VK_COLOR_COMPONENT_A_BIT;
        // 
        // VkPipelineColorBlendStateCreateInfo colorBlendStateInfo { };
        // colorBlendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        // colorBlendStateInfo.attachmentCount = 1;
        // colorBlendStateInfo.pAttachments = &colorBlendAttachment;
        // 
        // Pipeline::Layout::Info pipelineLayoutInfo;
        // pipelineLayoutInfo.setLayoutCount = 1;
        // pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout->handle();
        // auto pipelineLayout = device->create<Pipeline::Layout>(pipelineLayoutInfo);
        // 
        // std::array<VkDynamicState, 2> dynamicStates {
        //     VK_DYNAMIC_STATE_VIEWPORT,
        //     VK_DYNAMIC_STATE_SCISSOR,
        // };
        // 
        // VkPipelineDynamicStateCreateInfo dynamicStateInfo { };
        // dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        // dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        // dynamicStateInfo.pDynamicStates = dynamicStates.data();
        // 
        // Pipeline::GraphicsInfo pipelineInfo;
        // pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        // pipelineInfo.pStages = shaderStages.data();
        // pipelineInfo.pVertexInputState = &vertexInputInfo;
        // pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
        // pipelineInfo.pViewportState = &viewportInfo;
        // pipelineInfo.pRasterizationState = &rasterizationInfo;
        // pipelineInfo.pMultisampleState = &multisampleInfo;
        // pipelineInfo.pDepthStencilState = &depthStencilInfo;
        // pipelineInfo.pColorBlendState = &colorBlendStateInfo;
        // pipelineInfo.pDynamicState = &dynamicStateInfo;
        // pipelineInfo.layout = *pipelineLayout;
        // pipelineInfo.renderPass = *renderPass;
        // pipelineInfo.subpass = 0;
        // auto pipeline = device->create<Pipeline>(pipelineInfo);
        // 
        // vertexShader.reset();
        // fragmentShader.reset();

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Create Command::Pool
        Command::Pool::Info commandPoolInfo;
        commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        commandPoolInfo.queueFamilyIndex = static_cast<uint32_t>(graphicsQueue.family_index());
        auto commandPool = device->create<Command::Pool>(commandPoolInfo);

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Create Image and Sampler

        // // FROM : https://pixabay.com/en/sea-ocean-turtle-wildlife-closeup-2361247/
        // // FROM : https://pixabay.com/en/phi-phi-islands-phuket-thailand-2538412/
        // // FROM : https://pixabay.com/en/portugal-mountains-landscape-beach-2537468/
        // // FROM : https://pixabay.com/en/statue-sculpture-figure-1275469/
        // auto filePath = "../../../examples/resources/images/statue.jpg";
        // auto imageCache = ImageReader::read_file(filePath);
        // 
        // Buffer::Info imageStagingBufferInfo;
        // imageStagingBufferInfo.size = static_cast<VkDeviceSize>(imageCache.data().size_bytes());
        // imageStagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        // auto imageStagingMemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        // auto imageStagingBuffer = device->create<Buffer>(imageStagingBufferInfo, imageStagingMemoryProperties);
        // imageStagingBuffer->write<uint8_t>(imageCache.data());
        // 
        // Image::Info imageInfo;
        // imageInfo.imageType = VK_IMAGE_TYPE_2D;
        // imageInfo.extent.width = imageCache.width();
        // imageInfo.extent.height = imageCache.height();
        // imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        // imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        // imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
        // imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        // auto image = device->create<Image>(imageInfo);
        // 
        // Memory::Info imageMemoryInfo;
        // auto imageMemoryRequirements = image->memory_requirements();
        // imageMemoryInfo.allocationSize = imageMemoryRequirements.size;
        // imageMemoryInfo.memoryTypeIndex = physicalDevice.find_memory_type_index(
        //     imageMemoryRequirements.memoryTypeBits,
        //     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        // );
        // 
        // auto imageMemory = device->allocate<Memory>(imageMemoryInfo);
        // image->bind_memory(imageMemory);
        // 
        // process_transient_command_buffer(
        //     *commandPool,
        //     graphicsQueue,
        //     [&](Command::Buffer& commandBuffer)
        //     {
        //         auto imageMemoryBarrier = create_layout_transition_barrier(
        //             *image,
        //             VK_IMAGE_LAYOUT_PREINITIALIZED,
        //             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        //         );
        // 
        //         vkCmdPipelineBarrier(
        //             commandBuffer,
        //             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        //             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        //             0,
        //             0, nullptr,
        //             0, nullptr,
        //             1, &imageMemoryBarrier
        //         );
        // 
        //         VkBufferImageCopy region { };
        //         region.bufferOffset = 0;
        //         region.bufferRowLength = 0;
        //         region.bufferImageHeight = 0;
        //         region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        //         region.imageSubresource.mipLevel = 0;
        //         region.imageSubresource.baseArrayLayer = 0;
        //         region.imageSubresource.layerCount = 1;
        //         region.imageOffset = { 0, 0, 0 };
        //         region.imageExtent = {
        //             imageCache.width(),
        //             imageCache.height(),
        //             1
        //         };
        // 
        //         vkCmdCopyBufferToImage(
        //             commandBuffer,
        //             *imageStagingBuffer,
        //             *image,
        //             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        //             1,
        //             &region
        //         );
        // 
        //         imageMemoryBarrier = create_layout_transition_barrier(
        //             *image,
        //             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        //             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        //         );
        // 
        //         vkCmdPipelineBarrier(
        //             commandBuffer,
        //             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        //             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        //             0,
        //             0, nullptr,
        //             0, nullptr,
        //             1, &imageMemoryBarrier
        //         );
        //     }
        // );
        // 
        // image->create<Image::View>();

        ShapeBlaster::Resources resources;
        resources.load(*device, *commandPool, graphicsQueue, swapchain->format());

        // auto image = resources.playerImage;
        // Sampler::Info samplerInfo;
        // samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        // samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        // samplerInfo.magFilter = VK_FILTER_NEAREST;
        // samplerInfo.minFilter = VK_FILTER_NEAREST;
        // auto sampler = device->create<Sampler>(samplerInfo);

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Create vertex and index Buffers

        // float w = static_cast<float>(image->extent().width);
        // float h = static_cast<float>(image->extent().height);
        // float a = 1.0f / std::max(w, h);
        // w = w * a * 0.5f;
        // h = h * a * 0.5f;
        // const std::vector<Vertex> vertices {
        //     { { -w, -h, 0 }, { 0, 0 }, { dst::Color::OrangeRed } },
        //     { {  w, -h, 0 }, { 1, 0 }, { dst::Color::BlueViolet } },
        //     { {  w,  h, 0 }, { 1, 1 }, { dst::Color::DodgerBlue } },
        //     { { -w,  h, 0 }, { 0, 1 }, { dst::Color::Goldenrod } },
        // 
        //     { { -w, -h, -0.5f }, { 0, 0 }, { dst::Color::OrangeRed } },
        //     { {  w, -h, -0.5f }, { 1, 0 }, { dst::Color::BlueViolet } },
        //     { {  w,  h, -0.5f }, { 1, 1 }, { dst::Color::DodgerBlue } },
        //     { { -w,  h, -0.5f }, { 0, 1 }, { dst::Color::Goldenrod } },
        // };
        // 
        // auto vertexBufferSize = static_cast<VkDeviceSize>(sizeof(vertices[0]) * vertices.size());
        // 
        // Buffer::Info vertexBufferInfo;
        // vertexBufferInfo.size = vertexBufferSize;
        // vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        // auto vertexMemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        // auto vertexBuffer = device->create<Buffer>(vertexBufferInfo, vertexMemoryProperties);
        // 
        // Buffer::Info stagingBufferInfo;
        // stagingBufferInfo.size = vertexBufferSize;
        // stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        // auto stagingMemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        // auto stagingBuffer = device->create<Buffer>(stagingBufferInfo, stagingMemoryProperties);
        // stagingBuffer->write<Vertex>(vertices);
        // 
        // process_transient_command_buffer(
        //     *commandPool,
        //     graphicsQueue,
        //     [&](Command::Buffer& commandBuffer)
        //     {
        //         VkBufferCopy copyInfo { };
        //         copyInfo.size = vertexBufferSize;
        //         commandBuffer.copy_buffer(*stagingBuffer, *vertexBuffer, vertexBufferSize);
        //     }
        // );
        // 
        // const std::vector<uint16_t> indices {
        //     0, 1, 2, 2, 3, 0,
        //     4, 5, 6, 6, 7, 4,
        // };
        // 
        // auto indexBufferSize = static_cast<VkDeviceSize>(sizeof(indices[0]) * indices.size());
        // 
        // Buffer::Info indexBufferInfo;
        // indexBufferInfo.size = indexBufferSize;
        // indexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        // auto indexMemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        // auto indexBuffer = device->create<Buffer>(vertexBufferInfo, vertexMemoryProperties);
        // stagingBuffer->write<uint16_t>(indices);
        // 
        // process_transient_command_buffer(
        //     *commandPool,
        //     graphicsQueue,
        //     [&](Command::Buffer& commandBuffer)
        //     {
        //         VkBufferCopy copyInfo { };
        //         copyInfo.size = indexBufferSize;
        //         commandBuffer.copy_buffer(*stagingBuffer, *indexBuffer, indexBufferSize);
        //     }
        // );

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Create uniform Buffer

        // VkDeviceSize uniformBufferSize = sizeof(UniformBuffer);
        // Buffer::Info uniformBufferInfo;
        // uniformBufferInfo.size = uniformBufferSize;
        // uniformBufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        // auto uniformMemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        // auto uniformBuffer = device->create<Buffer>(uniformBufferInfo, uniformMemoryProperties);

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Create Descriptor::Pool and Descriptor::Set

        // std::array<VkDescriptorPoolSize, 2> descriptorPoolSizes;
        // descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        // descriptorPoolSizes[0].descriptorCount = 1;
        // descriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        // descriptorPoolSizes[1].descriptorCount = 1;
        // 
        // Descriptor::Pool::Info descriptorPoolInfo;
        // descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
        // descriptorPoolInfo.pPoolSizes = descriptorPoolSizes.data();
        // descriptorPoolInfo.maxSets = 1;
        // auto desciptorPool = device->create<Descriptor::Pool>(descriptorPoolInfo);
        // 
        // Descriptor::Set::Info descriptorSetInfo;
        // descriptorSetInfo.descriptorPool = *desciptorPool;
        // descriptorSetInfo.descriptorSetCount = 1;
        // descriptorSetInfo.pSetLayouts = &descriptorSetLayout->handle();
        // auto descriptorSet = desciptorPool->allocate<Descriptor::Set>(descriptorSetInfo);
        // 
        // VkDescriptorBufferInfo descriptorBufferInfo { };
        // descriptorBufferInfo.buffer = *uniformBuffer;
        // descriptorBufferInfo.offset = 0;
        // descriptorBufferInfo.range = sizeof(UniformBuffer);
        // 
        // VkDescriptorImageInfo descriptorImageInfo { };
        // descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        // descriptorImageInfo.imageView = *image->views()[0];
        // descriptorImageInfo.sampler = *sampler;
        // 
        // std::array<VkWriteDescriptorSet, 2> descriptorWrites { };
        // descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        // descriptorWrites[0].dstSet = *descriptorSet;
        // descriptorWrites[0].dstBinding = 0;
        // descriptorWrites[0].dstArrayElement = 0;
        // descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        // descriptorWrites[0].descriptorCount = 1;
        // descriptorWrites[0].pBufferInfo = &descriptorBufferInfo;
        // 
        // descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        // descriptorWrites[1].dstSet = *descriptorSet;
        // descriptorWrites[1].dstBinding = 1;
        // descriptorWrites[1].dstArrayElement = 0;
        // descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        // descriptorWrites[1].descriptorCount = 1;
        // descriptorWrites[1].pImageInfo = &descriptorImageInfo;
        // vkUpdateDescriptorSets(
        //     *device,
        //     static_cast<uint32_t>(descriptorWrites.size()),
        //     descriptorWrites.data(),
        //     0,
        //     nullptr
        // );

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Create Command::Buffers
        for (size_t i = 0; i < swapchain->images().size(); ++i) {
            commandPool->allocate<Command::Buffer>();
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Create Sempahores
        auto imageSemaphore = device->create<Semaphore>();
        auto renderSemaphore = device->create<Semaphore>();

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Render
        bool createFramebuffersAndDepthBuffer = true;
        bool recordCommandBuffers = true;
        swapchain->on_resized =
        [&](const SwapchainKHR&)
        {
            // NOTE : If our Window size changes, our Surface and Swapchain's Image
            //        sizes will also change.  When this occurs we need to recreate
            //        our Framebuffers and re-record our Command::Buffers.
            createFramebuffersAndDepthBuffer = true;
            recordCommandBuffers = true;
        };

        window->name("Dynamic_Static VK.07.ShapeBlaster");

        std::vector<std::shared_ptr<Framebuffer>> framebuffers;
        framebuffers.reserve(swapchain->images().size());

        std::shared_ptr<Image> depthBuffer;
        std::shared_ptr<Memory> depthBufferMemory;

        std::array<VkFormat, 3> depthFormats {
            VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT
        };

        auto depthFormat = physicalDevice.find_supported_format(
            depthFormats,
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );

        dst::Clock clock;
        float angle = 0;
        bool running = true;
        while (running) {
            Window::update();
            auto quitKey = dst::Keyboard::Key::Escape;
            if (window->input().keyboard().down(quitKey)) {
                running = false;
            }

            clock.update();
            float dt = clock.elapsed<dst::Second<float>>();
            angle += 90.0f * clock.elapsed<dst::Second<float>>();

            ShapeBlaster::Sprite::UniformBuffer ubo;
            ubo.world = dst::Matrix4x4::Identity;
            ubo.world = dst::Matrix4x4::create_rotation(
                dst::to_radians(angle),
                dst::Vector3::UnitZ
            );

            float cameraSpeed = 8;
            static float x = 0;
            static float y = 0;
            static float z = 64;
            // std::cout << z << std::endl;
            if (window->input().keyboard().down(dst::Keyboard::Key::W)) {
                z -= cameraSpeed * dt;
            }

            if (window->input().keyboard().down(dst::Keyboard::Key::S)) {
                z += cameraSpeed * dt;
            }

            if (window->input().keyboard().down(dst::Keyboard::Key::A)) {

            }

            if (window->input().keyboard().down(dst::Keyboard::Key::D)) {

            }

            if (window->input().keyboard().down(dst::Keyboard::Key::Q)) {

            }

            if (window->input().keyboard().down(dst::Keyboard::Key::E)) {

            }

            ubo.view = dst::Matrix4x4::create_view(
                { x, y, z },
                dst::Vector3::Zero,
                dst::Vector3::UnitY
            );

            float w = static_cast<float>(swapchain->extent().width);
            float h = static_cast<float>(swapchain->extent().height);
            ubo.projection = dst::Matrix4x4::create_perspective(
                dst::to_radians(30.0f),
                w / h,
                0.01f,
                100.0f
            );

            ubo.projection[1][1] *= -1;

            // uniformBuffer->write<UniformBuffer>(std::array<UniformBuffer, 1> { ubo });
            resources.playerSprite.uniformBuffer->write<ShapeBlaster::Sprite::UniformBuffer>(
                std::array<ShapeBlaster::Sprite::UniformBuffer, 1> { ubo }
            );

            presentQueue.wait_idle();
            swapchain->update();
            if (swapchain->valid()) {
                auto imageIndex = static_cast<uint32_t>(swapchain->next_image(*imageSemaphore));

                if (createFramebuffersAndDepthBuffer) {
                    createFramebuffersAndDepthBuffer = false;
                    recordCommandBuffers = true;

                    auto extent = swapchain->extent();

                    Image::Info depthBufferInfo;
                    depthBufferInfo.imageType = VK_IMAGE_TYPE_2D;
                    depthBufferInfo.extent.width = extent.width;
                    depthBufferInfo.extent.height = extent.height;
                    depthBufferInfo.format = depthFormat;
                    depthBufferInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
                    depthBufferInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
                    depthBufferInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
                    depthBuffer = device->create<Image>(depthBufferInfo);

                    Memory::Info depthBufferMemoryInfo;
                    auto depthBufferMemoryRequirements = depthBuffer->memory_requirements();
                    depthBufferMemoryInfo.allocationSize = depthBufferMemoryRequirements.size;
                    depthBufferMemoryInfo.memoryTypeIndex = physicalDevice.find_memory_type_index(
                        depthBufferMemoryRequirements.memoryTypeBits,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                    );

                    depthBufferMemory = device->allocate<Memory>(depthBufferMemoryInfo);
                    depthBuffer->bind_memory(depthBufferMemory);
                    depthBuffer->create<Image::View>();

                    framebuffers.clear();
                    framebuffers.reserve(swapchain->images().size());
                    for (const auto& image : swapchain->images()) {
                        std::array<VkImageView, 2> attachments {
                            *image->views()[0],
                            *depthBuffer->views()[0]
                        };

                        Framebuffer::Info framebufferInfo;
                        framebufferInfo.renderPass = *resources.mRenderPass; // *renderPass;
                        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
                        framebufferInfo.pAttachments = attachments.data();
                        framebufferInfo.width = extent.width;
                        framebufferInfo.height = extent.height;
                        framebufferInfo.layers = 1;
                        framebuffers.push_back(device->create<Framebuffer>(framebufferInfo));
                    }
                }

                if (recordCommandBuffers) {
                    recordCommandBuffers = false;
                    for (size_t i = 0; i < framebuffers.size(); ++i) {
                        auto& commandBuffer = commandPool->buffers()[i];

                        Command::Buffer::BeginInfo beginInfo;
                        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
                        commandBuffer->begin(beginInfo);

                        std::array<VkClearValue, 2> clearValues;
                        clearValues[0].color = { 0.2f, 0.2f, 0.2f, 1 };
                        clearValues[1].depthStencil = { 1, 0 };
                        RenderPass::BeginInfo renderPassBeginInfo;
                        renderPassBeginInfo.renderPass = *resources.mRenderPass; // *renderPass;
                        renderPassBeginInfo.framebuffer = *framebuffers[i];
                        renderPassBeginInfo.renderArea.extent = swapchain->extent();
                        renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
                        renderPassBeginInfo.pClearValues = clearValues.data();
                        commandBuffer->begin_render_pass(renderPassBeginInfo);

                        // commandBuffer->bind_pipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
                        commandBuffer->bind_pipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, *resources.mPipeline);

                        VkViewport viewport { };
                        viewport.width = static_cast<float>(swapchain->extent().width);
                        viewport.height = static_cast<float>(swapchain->extent().height);
                        viewport.minDepth = 0;
                        viewport.maxDepth = 1;
                        commandBuffer->set_viewport(viewport);

                        VkRect2D scissor { };
                        scissor.extent = swapchain->extent();
                        commandBuffer->set_scissor(scissor);
                        // commandBuffer->bind_descriptor_set(*descriptorSet, *pipelineLayout);
                        commandBuffer->bind_descriptor_set(
                            *resources.playerSprite.descriptorSet,
                            *resources.mPipelineLayout
                        );

                        // commandBuffer->bind_vertex_buffer(*vertexBuffer);
                        // commandBuffer->bind_index_buffer(*indexBuffer);
                        // commandBuffer->draw_indexed(indices.size());

                        commandBuffer->bind_vertex_buffer(*resources.quadVertexBuffer);
                        commandBuffer->bind_index_buffer(*resources.quadIndexBuffer);
                        commandBuffer->draw_indexed(ShapeBlaster::Resources::QuadIndexCount);

                        commandBuffer->end_render_pass();
                        commandBuffer->end();
                    }
                }

                Queue::SubmitInfo submitInfo;
                VkPipelineStageFlags waitStages[] { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
                submitInfo.waitSemaphoreCount = 1;
                submitInfo.pWaitSemaphores = &imageSemaphore->handle();
                submitInfo.pWaitDstStageMask = waitStages;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &commandPool->buffers()[imageIndex]->handle();
                submitInfo.signalSemaphoreCount = 1;
                submitInfo.pSignalSemaphores = &renderSemaphore->handle();
                graphicsQueue.submit(submitInfo);

                Queue::PresentInfoKHR presentInfo;
                presentInfo.waitSemaphoreCount = 1;
                presentInfo.pWaitSemaphores = &renderSemaphore->handle();
                presentInfo.swapchainCount = 1;
                presentInfo.pSwapchains = &swapchain->handle();
                presentInfo.pImageIndices = &imageIndex;
                presentQueue.present(presentInfo);
            }

            window->swap_buffers();
        }

        device->wait_idle();
    }

    return 0;
}
