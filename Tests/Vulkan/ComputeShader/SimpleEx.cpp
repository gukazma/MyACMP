
#include <gtest/gtest.h>
#include "Shader/shader_comp.h"
#include "Shader/shader_comp.h"
#include <MyACMP/Vulkan/Buffer.hpp>
#include <fstream>
#include <memory>
#include <optional>
#include <vulkan/vulkan.hpp>
struct QueueInfo
{
    std::optional<uint32_t> family;
} queueInfo;

vk::Instance            instance;
vk::PhysicalDevice      phyDevice;
vk::Device              device;
vk::Queue               computeQueue;
vk::DescriptorSetLayout setLayout;
vk::PipelineLayout      layout;
vk::Pipeline            computePipeline;
vk::CommandPool         cmdPool;
vk::DescriptorPool      descriptorPool;
std::unique_ptr<Buffer> storageBuffer;

vk::Instance            CreateInstance();
vk::PhysicalDevice      PickupPhysicalDevice();
vk::Device              CreateDevice();
vk::DescriptorSetLayout CreateSetLayout();
vk::PipelineLayout      CreatePiplineLayout();
vk::Pipeline            CreateComputePipline();
vk::CommandPool         CreateCmdPool();
vk::DescriptorPool      CreateDescriptorPool();
vk::DescriptorSet       AllocateDescriptorSet();
vk::CommandBuffer       AllocateCmdBuf();
void                    UpdateDescriptorSet(vk::DescriptorSet);

TEST(ComputeShader, SimpleEx)
{
    instance        = CreateInstance();
    phyDevice       = PickupPhysicalDevice();
    device          = CreateDevice();
    computeQueue    = device.getQueue(queueInfo.family.value(), 0);
    setLayout       = CreateSetLayout();
    layout          = CreatePiplineLayout();
    computePipeline = CreateComputePipline();
    cmdPool         = CreateCmdPool();
    descriptorPool  = CreateDescriptorPool();

    storageBuffer.reset(new Buffer(
        phyDevice,
        device,
        vk::BufferUsageFlagBits::eStorageBuffer,
        sizeof(unsigned int) * 4,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));

    auto set = AllocateDescriptorSet();
    auto cmd = AllocateCmdBuf();

    UpdateDescriptorSet(set);

    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    cmd.begin(beginInfo);
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, layout, 0, {set}, {});
    cmd.dispatch(1, 1, 1);
    cmd.end();

    vk::SubmitInfo info;
    info.setCommandBuffers(cmd);
    computeQueue.submit(info);

    device.waitIdle();

    unsigned int data[4] = {0};
    memcpy(data, storageBuffer->map, sizeof(data));

    for (auto value : data) {
        std::cout << value << std::endl;
    }

    storageBuffer.reset();
    device.destroyCommandPool(cmdPool);
    device.destroyDescriptorPool(descriptorPool);
    device.destroyPipeline(computePipeline);
    device.destroyPipelineLayout(layout);
    device.destroyDescriptorSetLayout(setLayout);
    device.destroy();
    instance.destroy();
}

vk::Instance CreateInstance()
{
    vk::InstanceCreateInfo   createInfo;
    std::vector<const char*> layers = {"VK_LAYER_KHRONOS_validation"};
    createInfo.setPEnabledLayerNames(layers);
    vk::ApplicationInfo appinfo;
    appinfo.setApiVersion(VK_API_VERSION_1_3);
    createInfo.setPApplicationInfo(&appinfo);

    return vk::createInstance(createInfo);
}
vk::PhysicalDevice PickupPhysicalDevice()
{
    return instance.enumeratePhysicalDevices()[0];
}

vk::Device CreateDevice()
{
    auto properties = phyDevice.getQueueFamilyProperties();
    for (size_t i = 0; i < properties.size(); i++) {
        if (properties[i].queueFlags & vk::QueueFlagBits::eCompute) {
            queueInfo.family = i;
        }
    }

    if (!queueInfo.family) {
        return nullptr;
    }


    vk::DeviceCreateInfo      createInfo;
    vk::DeviceQueueCreateInfo queueCreateInfo;
    float                     prioritiy = 1.0;
    queueCreateInfo.setQueueFamilyIndex(queueInfo.family.value())
        .setQueuePriorities(prioritiy)
        .setQueueCount(1);
    createInfo.setQueueCreateInfos(queueCreateInfo);
    return phyDevice.createDevice(createInfo);
}

vk::DescriptorSetLayout CreateSetLayout()
{
    vk::DescriptorSetLayoutCreateInfo createInfo;
    vk::DescriptorSetLayoutBinding    binding;
    binding.setBinding(0)
        .setDescriptorType(vk::DescriptorType::eStorageBuffer)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eCompute);
    createInfo.setBindings(binding);
    return device.createDescriptorSetLayout(createInfo);
}

vk::PipelineLayout CreatePiplineLayout()
{
    vk::PipelineLayoutCreateInfo createInfo;
    createInfo.setSetLayouts(setLayout);

    return device.createPipelineLayout(createInfo);
}

vk::Pipeline CreateComputePipline()
{
    vk::ShaderModuleCreateInfo moduleCreateInfo;
    moduleCreateInfo.pCode    = shader_comp;
    moduleCreateInfo.codeSize = sizeof(shader_comp);

    vk::ShaderModule                  module = device.createShaderModule(moduleCreateInfo);
    vk::PipelineShaderStageCreateInfo stageCreateInfo;
    stageCreateInfo.setModule(module).setPName("main").setStage(vk::ShaderStageFlagBits::eCompute);

    vk::ComputePipelineCreateInfo createInfo;
    createInfo.setLayout(layout).setStage(stageCreateInfo);

    auto result = device.createComputePipeline({}, createInfo);
    if (result.result != vk::Result::eSuccess) {
        std::cout << "create compute pipline failed" << std::endl;
    }

    device.destroyShaderModule(module);
    return result.value;
}

vk::CommandPool CreateCmdPool()
{
    vk::CommandPoolCreateInfo createInfo;
    createInfo.setQueueFamilyIndex(queueInfo.family.value())
        .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    return device.createCommandPool(createInfo);
}

vk::DescriptorPool CreateDescriptorPool()
{
    vk::DescriptorPoolCreateInfo createInfo;
    vk::DescriptorPoolSize       size;
    size.setType(vk::DescriptorType::eStorageBuffer).setDescriptorCount(1);
    createInfo.setMaxSets(1).setPoolSizes(size);
    return device.createDescriptorPool(createInfo);
}

vk::DescriptorSet AllocateDescriptorSet()
{
    std::array                    layouts = {setLayout};
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.setDescriptorPool(descriptorPool).setSetLayouts(layouts).setDescriptorSetCount(1);
    return device.allocateDescriptorSets(allocInfo)[0];
}

vk::CommandBuffer AllocateCmdBuf()
{
    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.setCommandBufferCount(1).setCommandPool(cmdPool).setLevel(
        vk::CommandBufferLevel::ePrimary);
    return device.allocateCommandBuffers(allocInfo)[0];
}

void UpdateDescriptorSet(vk::DescriptorSet set)
{
    vk::DescriptorBufferInfo bufferInfo;
    bufferInfo.setBuffer(storageBuffer->buffer).setOffset(0).setRange(storageBuffer->size);

    vk::WriteDescriptorSet writer;
    writer.setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eStorageBuffer)
        .setDstBinding(0)
        .setBufferInfo(bufferInfo)
        .setDstSet(set);
    device.updateDescriptorSets(writer, {});
}