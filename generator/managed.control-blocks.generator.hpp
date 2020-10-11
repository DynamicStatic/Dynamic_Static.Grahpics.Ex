
/*
==========================================
  Copyright (c) 2020 Dynamic_Static
    Patrick Purcell
      Licensed under the MIT license
    http://opensource.org/licenses/MIT
==========================================
*/

#include "dynamic_static/cpp-generator.hpp"
#include "dynamic_static/vk-xml-parser.hpp"
#include "utilities.hpp"

namespace dst {
namespace vk {
namespace cppgen {

inline void generate_managed_control_block_declarations(const xml::Manifest& xmlManifest)
{
    using namespace dst::cppgen;
    using namespace dst::vk::xml;
    std::filesystem::path includePath(DYNAMIC_STATIC_GRAPHICS_VULKAN_GENERATED_INCLUDE_PATH);
    File(includePath / "managed.control-blocks.hpp") << SourceBlock(R"(
    /*
    ==========================================
        Copyright (c) 2020 Dynamic_Static
        Patrick Purcell
            Licensed under the MIT license
        http://opensource.org/licenses/MIT
    ==========================================
    */

    #pragma once

    #include "dynamic_static/graphics/vulkan/detail/managed.hpp"
    #include "dynamic_static/graphics/vulkan/defines.hpp"

    #include <cassert>
    #include <tuple>
    #include <utility>
    #include <vector>

    namespace dst {
    namespace vk {

    $<HANDLES:"\n">
    $<COMPILE_GUARDS>
    #ifdef ${COMPILE_GUARD}
    $</>
    template <>
    class Managed<${HANDLE_TYPE_NAME}>::ControlBlock
    {
    public:
        $<CREATE_FUNCTIONS>
        $<COMPILE_GUARDS>
        #ifdef ${COMPILE_GUARD}
        $</>
        static VkResult ${MANAGED_CREATE_FUNCTION_NAME}($<PARAMETERS:", ">${MANAGED_PARAMETER_TYPE} ${MANAGED_PARAMETER_NAME}$</>);
        $<COMPILE_GUARDS:reverse="true">
        #endif // ${COMPILE_GUARD}
        $</>
        $</"\n">
        ~ControlBlock();

        template <typename T>
        inline const T& get() const
        {
            return std::get<T>(mFields);
        }

    private:
        template <typename T>
        inline void set(T&& field)
        {
            std::get<T>(mFields) = std::move(field);
        }

        std::tuple<
            VkObjectType,
            $<PARENT_HANDLES>
            Managed<${PARENT_HANDLE_TYPE_NAME}>,
            $</>
            $<CREATE_INFO_STRUCTURES>
            $<COMPILE_GUARDS>
            #ifdef ${COMPILE_GUARD}
            $</>
            Managed<${STRUCTURE_TYPE_NAME}>,
            $<COMPILE_GUARDS:reverse="true">
            #endif // ${COMPILE_GUARD}
            $</>
            $</>
            $<condition="HAS_ALLOCATOR">
            VkAllocationCallbacks,
            $</>
            ${HANDLE_TYPE_NAME}
        > mFields;
    };
    $<COMPILE_GUARDS:reverse="true">
    #endif // ${COMPILE_GUARD}
    $</>
    $</>

    } // namespace vk
    } // namespace dst
    )", {
        get_handle_source_blocks(xmlManifest)
    });
}

inline void generate_managed_control_block_definitions(const xml::Manifest& xmlManifest)
{
    using namespace dst::cppgen;
    using namespace dst::vk::xml;
    std::filesystem::path sourcePath(DYNAMIC_STATIC_GRAPHICS_VULKAN_GENERATED_SOURCE_PATH);
    File(sourcePath / "managed.control-blocks.cpp") << SourceBlock(R"(
    /*
    ==========================================
        Copyright (c) 2020 Dynamic_Static
        Patrick Purcell
            Licensed under the MIT license
        http://opensource.org/licenses/MIT
    ==========================================
    */

    #include "dynamic_static/graphics/vulkan/generated/managed.control-blocks.hpp"
    #include "dynamic_static/graphics/vulkan/managed.hpp"

    namespace dst {
    namespace vk {

    $<HANDLES:"\n">
    ${OPEN_MANUALLY_IMPLEMENTED_COMPILE_GUARD}
    $<COMPILE_GUARDS>
    #ifdef ${COMPILE_GUARD}
    $</>
    $<CREATE_FUNCTIONS>
    $<COMPILE_GUARDS>
    #ifdef ${COMPILE_GUARD}
    $</>
    VkResult Managed<${HANDLE_TYPE_NAME}>::ControlBlock::${MANAGED_CREATE_FUNCTION_NAME}($<PARAMETERS:", ">${MANAGED_PARAMETER_TYPE} ${MANAGED_PARAMETER_NAME}$</>)
    {
        auto vkResult = VK_ERROR_INITIALIZATION_FAILED;
        if (${MANAGED_HANDLE_PARAMETER_NAME}) {
            ${MANAGED_HANDLE_PARAMETER_NAME}->reset();
            ${HANDLE_TYPE_NAME} vkHandle = VK_NULL_HANDLE;
            vkResult = ${CREATE_FUNCTION_NAME}($<PARAMETERS:", ">${VK_CALL_ARGUMENT}$</>);
            if (vkResult == VK_SUCCESS) {
                ${MANAGED_HANDLE_PARAMETER_NAME}->mVkHandle = vkHandle;
                ${MANAGED_HANDLE_PARAMETER_NAME}->mspControlBlock = std::make_shared<Managed<${HANDLE_TYPE_NAME}>::ControlBlock>();
                ${MANAGED_HANDLE_PARAMETER_NAME}->mspControlBlock->set(${HANDLE_OBJECT_TYPE});
                $<PARAMETERS>
                $<condition="PARENT_PARAMETER">
                ${MANAGED_HANDLE_PARAMETER_NAME}->mspControlBlock->set(std::move(Managed<${MANAGED_PARAMETER_UNQUALIFIED_TYPE}>(${MANAGED_PARAMETER_NAME})));
                $</>
                $<condition="CREATE_INFO_PARAMETER">
                ${MANAGED_HANDLE_PARAMETER_NAME}->mspControlBlock->set(std::move(Managed<${MANAGED_PARAMETER_UNQUALIFIED_TYPE}>(*${MANAGED_PARAMETER_NAME})));
                $</>
                $</>
                $<condition="HAS_ALLOCATOR">
                ${MANAGED_HANDLE_PARAMETER_NAME}->mspControlBlock->set(std::move(pAllocator ? *pAllocator : VkAllocationCallbacks { }));
                $</>
                ${MANAGED_HANDLE_PARAMETER_NAME}->mspControlBlock->set(std::move(vkHandle));
            }
        }
        return vkResult;
    }
    $<COMPILE_GUARDS:reverse="true">
    #endif // ${COMPILE_GUARD}
    $</>
    $</"\n">
    Managed<${HANDLE_TYPE_NAME}>::ControlBlock::~ControlBlock()
    {
        $<DESTROY_FUNCTIONS>
        auto vkHandle = get<${HANDLE_TYPE_NAME}>();
        if (vkHandle) {
            $<PARENT_PARAMETERS>
            auto vk${STRIP_VK_PARENT_HANDLE_PARAMETER_TYPE_NAME} = *get<Managed<${PARENT_HANDLE_PARAMETER_TYPE_NAME}>>();
            assert(vk${STRIP_VK_PARENT_HANDLE_PARAMETER_TYPE_NAME});
            $</>
            ${VK_DESTROY_FUNCTION_NAME}($<PARENT_PARAMETERS:", ">vk${STRIP_VK_PARENT_HANDLE_PARAMETER_TYPE_NAME}$</", ">vkHandle, get<VkAllocationCallbacks>().pfnFree ? &get<VkAllocationCallbacks>() : nullptr);
        }
        $</>
    }
    $<COMPILE_GUARDS:reverse="true">
    #endif // ${COMPILE_GUARD}
    $</>
    ${CLOSE_MANUALLY_IMPLEMENTED_COMPILE_GUARD}
    $</>

    } // namespace vk
    } // namespace dst
    )", {
        get_handle_source_blocks(xmlManifest)
    });
}

inline void generate_managed_handles(const xml::Manifest& xmlManifest)
{
    generate_managed_control_block_declarations(xmlManifest);
    generate_managed_control_block_definitions(xmlManifest);
}

} // namespace cppgen
} // namespace vk
} // namespace dst