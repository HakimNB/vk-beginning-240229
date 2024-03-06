/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android/asset_manager.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <assert.h>
#include <vulkan/vulkan.h>

#include <array>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

/**
 * HelloVK contains the core of Vulkan pipeline setup. It includes recording
 * draw commands as well as screen clearing during the render pass.
 *
 * Please refer to: https://vulkan-tutorial.com/ for a gentle Vulkan
 * introduction.
 */

namespace vkt {
#define LOG_TAG "hellovkjni"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define VK_CHECK(x)                           \
  do {                                        \
    VkResult err = x;                         \
    if (err) {                                \
      LOGE("Detected Vulkan error: %d", err); \
      abort();                                \
    }                                         \
  } while (0)

const int MAX_FRAMES_IN_FLIGHT = 2;

// struct UniformBufferObject {
//   glm::mat4 mvp;
// };

struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;
  bool isComplete() {
    return graphicsFamily.has_value() && presentFamily.has_value();
  }
};

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

struct ANativeWindowDeleter {
  void operator()(ANativeWindow *window) { ANativeWindow_release(window); }
};

std::vector<uint8_t> LoadBinaryFileToVector(const char *file_path,
                                            AAssetManager *assetManager) {
  std::vector<uint8_t> file_content;
  assert(assetManager);
  AAsset *file =
      AAssetManager_open(assetManager, file_path, AASSET_MODE_BUFFER);
  size_t file_length = AAsset_getLength(file);

  file_content.resize(file_length);

  AAsset_read(file, file_content.data(), file_length);
  AAsset_close(file);
  return file_content;
}

const char *toStringMessageSeverity(VkDebugUtilsMessageSeverityFlagBitsEXT s) {
  switch (s) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
      return "VERBOSE";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
      return "ERROR";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
      return "WARNING";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
      return "INFO";
    default:
      return "UNKNOWN";
  }
}
const char *toStringMessageType(VkDebugUtilsMessageTypeFlagsEXT s) {
  if (s == (VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT))
    return "General | Validation | Performance";
  if (s == (VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT))
    return "Validation | Performance";
  if (s == (VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT))
    return "General | Performance";
  if (s == (VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT))
    return "Performance";
  if (s == (VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT))
    return "General | Validation";
  if (s == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) return "Validation";
  if (s == VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) return "General";
  return "Unknown";
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void * /* pUserData */) {
  auto ms = toStringMessageSeverity(messageSeverity);
  auto mt = toStringMessageType(messageType);
  printf("[%s: %s]\n%s\n", ms, mt, pCallbackData->pMessage);

  return VK_FALSE;
}

static void populateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
  createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = debugCallback;
}

static VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

static void DestroyDebugUtilsMessengerEXT(
    VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks *pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
  }
}

class HelloVK {
 public:
  void initVulkan();
  void render();
  void cleanup();
  void cleanupSwapChain();
  void reset(ANativeWindow *newWindow, AAssetManager *newManager);
  bool initialized = false;

 private:
  void createDevice();
  void createInstance();
  void createSurface();
  void setupDebugMessenger();
  void pickPhysicalDevice();
  void createLogicalDeviceAndQueue();
  void createSwapChain();
  // void createImageViews();
  // void createTextureImage();
  // void decodeImage();
  // void createTextureImageViews();
  // void createTextureSampler();
  // void copyBufferToImage();
  // void createRenderPass();
  // void createDescriptorSetLayout();
  // void createGraphicsPipeline();
  // void createFramebuffers();
  // void createCommandPool();
  // void createCommandBuffer();
  void createSyncObjects();
  QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
  bool checkDeviceExtensionSupport(VkPhysicalDevice device);
  bool isDeviceSuitable(VkPhysicalDevice device);
  bool checkValidationLayerSupport();
  std::vector<const char *> getRequiredExtensions(bool enableValidation);
  SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);
  VkShaderModule createShaderModule(const std::vector<uint8_t> &code);
  void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
  void recreateSwapChain();
  void onOrientationChange();
  uint32_t findMemoryType(uint32_t typeFilter,
                          VkMemoryPropertyFlags properties);
  // void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
  //                   VkMemoryPropertyFlags properties, VkBuffer &buffer,
  //                   VkDeviceMemory &bufferMemory);
  // void createUniformBuffers();
  // void updateUniformBuffer(uint32_t currentImage);
  // void createDescriptorPool();
  // void createDescriptorSets();
  void establishDisplaySizeIdentity();

  /*
   * In order to enable validation layer toggle this to true and
   * follow the README.md instructions concerning the validation
   * layers. You will be required to add separate vulkan validation
   * '*.so' files in order to enable this.
   *
   * The validation layers are not shipped with the APK as they are sizeable.
   */
  bool enableValidationLayers = false;

  const std::vector<const char *> validationLayers = {
      "VK_LAYER_KHRONOS_validation"};
  const std::vector<const char *> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  std::unique_ptr<ANativeWindow, ANativeWindowDeleter> window;
  AAssetManager *assetManager;

  VkInstance instance;
  VkDebugUtilsMessengerEXT debugMessenger;

  VkSurfaceKHR surface;

  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device;

  VkSwapchainKHR swapChain;
  std::vector<VkImage> swapChainImages;
  VkFormat swapChainImageFormat;
  VkExtent2D swapChainExtent;
  VkExtent2D displaySizeIdentity;
  std::vector<VkImageView> swapChainImageViews;
  std::vector<VkFramebuffer> swapChainFramebuffers;
  VkCommandPool commandPool;
  std::vector<VkCommandBuffer> commandBuffers;

  VkQueue graphicsQueue;
  VkQueue presentQueue;

  VkRenderPass renderPass;
  VkDescriptorSetLayout descriptorSetLayout;
  VkPipelineLayout pipelineLayout;
  VkPipeline graphicsPipeline;

  std::vector<VkBuffer> uniformBuffers;
  std::vector<VkDeviceMemory> uniformBuffersMemory;

  std::vector<VkSemaphore> imageAvailableSemaphores;
  std::vector<VkSemaphore> renderFinishedSemaphores;
  std::vector<VkFence> inFlightFences;
  VkDescriptorPool descriptorPool;
  std::vector<VkDescriptorSet> descriptorSets;

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingMemory;
  int textureWidth, textureHeight, textureChannels;
  VkImage textureImage;
  VkDeviceMemory textureImageMemory;
  VkImageView textureImageView;
  VkSampler textureSampler;

  uint32_t currentFrame = 0;
  bool orientationChanged = false;
  VkSurfaceTransformFlagBitsKHR pretransformFlag;
};

void HelloVK::initVulkan() {
  createInstance();
  createSurface();
  pickPhysicalDevice();
  createLogicalDeviceAndQueue();
  setupDebugMessenger();
  establishDisplaySizeIdentity();
  createSwapChain();
  // createImageViews();
  // createRenderPass();
  // createDescriptorSetLayout();
  // createGraphicsPipeline();
  // createFramebuffers();
  // createCommandPool();
  // createCommandBuffer();
  // decodeImage();
  // createTextureImage();
  // copyBufferToImage();
  // createTextureImageViews();
  // createTextureSampler();
  // createUniformBuffers();
  // createDescriptorPool();
  // createDescriptorSets();
  createSyncObjects();
  initialized = true;
}

/*
 *	Create a buffer with specified usage and memory properties
 *	i.e a uniform buffer which uses HOST_COHERENT memory
 *  Upon creation, these buffers will list memory requirements which need to be
 *  satisfied by the device in use in order to be created.
 */
// void HelloVK::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
//                            VkMemoryPropertyFlags properties, VkBuffer &buffer,
//                            VkDeviceMemory &bufferMemory) {
//   VkBufferCreateInfo bufferInfo{};
//   bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
//   bufferInfo.size = size;
//   bufferInfo.usage = usage;
//   bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

//   VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer));

//   VkMemoryRequirements memRequirements;
//   vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

//   VkMemoryAllocateInfo allocInfo{};
//   allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
//   allocInfo.allocationSize = memRequirements.size;
//   allocInfo.memoryTypeIndex =
//       findMemoryType(memRequirements.memoryTypeBits, properties);

//   VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory));

//   vkBindBufferMemory(device, buffer, bufferMemory, 0);
// }

/*
 * Finds the index of the memory heap which matches a particular buffer's memory
 * requirements. Vulkan manages these requirements as a bitset, in this case
 * expressed through a uint32_t.
 */
uint32_t HelloVK::findMemoryType(uint32_t typeFilter,
                                 VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }

  assert(false);  // failed to find suitable memory type!
  return -1;
}

// void HelloVK::createUniformBuffers() {
//   VkDeviceSize bufferSize = sizeof(UniformBufferObject);

//   uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
//   uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);

//   for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
//     createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
//                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
//                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//                  uniformBuffers[i], uniformBuffersMemory[i]);
//   }
// }

// void HelloVK::createDescriptorSetLayout() {
//   VkDescriptorSetLayoutBinding uboLayoutBinding{};
//   uboLayoutBinding.binding = 0;
//   uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
//   uboLayoutBinding.descriptorCount = 1;
//   uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
//   uboLayoutBinding.pImmutableSamplers = nullptr;

//   // Combined image sampler layout binding
//   VkDescriptorSetLayoutBinding samplerLayoutBinding{};
//   samplerLayoutBinding.binding = 1;
//   samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//   samplerLayoutBinding.descriptorCount = 1;
//   samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
//   samplerLayoutBinding.pImmutableSamplers = nullptr;

//   std::array<VkDescriptorSetLayoutBinding, 2> bindings =
//       {uboLayoutBinding, samplerLayoutBinding};

//   VkDescriptorSetLayoutCreateInfo layoutInfo{};
//   layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
//   layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
//   layoutInfo.pBindings = bindings.data();

//   VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
//                                        &descriptorSetLayout));
// }

void HelloVK::reset(ANativeWindow *newWindow, AAssetManager *newManager) {
  window.reset(newWindow);
  assetManager = newManager;
  // if (initialized) {
  //   createSurface();
  //   recreateSwapChain();
  // }
}

void HelloVK::recreateSwapChain() {
  vkDeviceWaitIdle(device);
  cleanupSwapChain();
  createSwapChain();
  // createImageViews();
  // createFramebuffers();
}

// void HelloVK::render() {
//   if (orientationChanged) {
//     onOrientationChange();
//   }

//   vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE,
//                   UINT64_MAX);
//   uint32_t imageIndex;
//   VkResult result = vkAcquireNextImageKHR(
//       device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame],
//       VK_NULL_HANDLE, &imageIndex);
//   if (result == VK_ERROR_OUT_OF_DATE_KHR) {
//     recreateSwapChain();
//     return;
//   }
//   assert(result == VK_SUCCESS ||
//          result == VK_SUBOPTIMAL_KHR);  // failed to acquire swap chain image
//   updateUniformBuffer(currentFrame);

//   vkResetFences(device, 1, &inFlightFences[currentFrame]);
//   vkResetCommandBuffer(commandBuffers[currentFrame], 0);

//   recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

//   VkSubmitInfo submitInfo{};
//   submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

//   VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
//   VkPipelineStageFlags waitStages[] = {
//       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
//   submitInfo.waitSemaphoreCount = 1;
//   submitInfo.pWaitSemaphores = waitSemaphores;
//   submitInfo.pWaitDstStageMask = waitStages;
//   submitInfo.commandBufferCount = 1;
//   submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
//   VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
//   submitInfo.signalSemaphoreCount = 1;
//   submitInfo.pSignalSemaphores = signalSemaphores;

//   VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo,
//                          inFlightFences[currentFrame]));

//   VkPresentInfoKHR presentInfo{};
//   presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

//   presentInfo.waitSemaphoreCount = 1;
//   presentInfo.pWaitSemaphores = signalSemaphores;

//   VkSwapchainKHR swapChains[] = {swapChain};
//   presentInfo.swapchainCount = 1;
//   presentInfo.pSwapchains = swapChains;
//   presentInfo.pImageIndices = &imageIndex;
//   presentInfo.pResults = nullptr;

//   result = vkQueuePresentKHR(presentQueue, &presentInfo);
//   if (result == VK_SUBOPTIMAL_KHR) {
//     orientationChanged = true;
//   } else if (result == VK_ERROR_OUT_OF_DATE_KHR) {
//     recreateSwapChain();
//   } else {
//     assert(result == VK_SUCCESS);  // failed to present swap chain image!
//   }
//   currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
// }

/*
 * getPrerotationMatrix handles screen rotation with 3 hardcoded rotation
 * matrices (detailed below). We skip the 180 degrees rotation.
 */
// void getPrerotationMatrix(const VkSurfaceCapabilitiesKHR &capabilities,
//                           const VkSurfaceTransformFlagBitsKHR &pretransformFlag,
//                           glm::mat4 &mat, float ratio) {
//   // mat is initialized to the identity matrix
//   mat = glm::mat4(1.0f);

//   // scale by screen ratio
//   mat = glm::scale(mat, glm::vec3(1.0f, ratio, 1.0f));

//   // rotate 1 degree every function call.
//   static float currentAngleDegrees = 0.0f;
//   currentAngleDegrees += 1.0f;
//   mat = glm::rotate(mat, glm::radians(currentAngleDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
// }

// void HelloVK::createDescriptorPool() {
//   VkDescriptorPoolSize poolSizes[2];
//   poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
//   poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
//   poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//   poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

//   VkDescriptorPoolCreateInfo poolInfo{};
//   poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
//   poolInfo.poolSizeCount = 2;
//   poolInfo.pPoolSizes = poolSizes;
//   poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 2;

//   VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool));
// }

// void HelloVK::createDescriptorSets() {
//   std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
//                                              descriptorSetLayout);
//   VkDescriptorSetAllocateInfo allocInfo{};
//   allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
//   allocInfo.descriptorPool = descriptorPool;
//   allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
//   allocInfo.pSetLayouts = layouts.data();

//   descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
//   VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()));

//   for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
//     VkDescriptorBufferInfo bufferInfo{};
//     bufferInfo.buffer = uniformBuffers[i];
//     bufferInfo.offset = 0;
//     bufferInfo.range = sizeof(UniformBufferObject);

//     VkDescriptorImageInfo imageInfo{};
//     imageInfo.imageView = textureImageView;
//     imageInfo.sampler = textureSampler;
//     imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

//     std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

//     // Uniform buffer
//     descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//     descriptorWrites[0].dstSet = descriptorSets[i];
//     descriptorWrites[0].dstBinding = 0;
//     descriptorWrites[0].dstArrayElement = 0;
//     descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
//     descriptorWrites[0].descriptorCount = 1;
//     descriptorWrites[0].pBufferInfo = &bufferInfo;

//     // Combined image sampler
//     descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//     descriptorWrites[1].dstSet = descriptorSets[i];
//     descriptorWrites[1].dstBinding = 1;
//     descriptorWrites[1].dstArrayElement = 0;
//     descriptorWrites[1].descriptorType =
//         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//     descriptorWrites[1].descriptorCount = 1;
//     descriptorWrites[1].pImageInfo = &imageInfo;

//     vkUpdateDescriptorSets(device,
//                            static_cast<uint32_t>(descriptorWrites.size()),
//                            descriptorWrites.data(), 0, nullptr);
//   }
// }

// void HelloVK::updateUniformBuffer(uint32_t currentImage) {
//   SwapChainSupportDetails swapChainSupport =
//       querySwapChainSupport(physicalDevice);
//   UniformBufferObject ubo{};
//   float ratio = (float)swapChainExtent.width / (float)swapChainExtent.height; 
//   getPrerotationMatrix(swapChainSupport.capabilities, pretransformFlag,
//                        ubo.mvp, ratio);
//   void *data;
//   vkMapMemory(device, uniformBuffersMemory[currentImage], 0, sizeof(ubo), 0,
//               &data);
//   memcpy(data, glm::value_ptr(ubo.mvp), sizeof(glm::mat4));
//   vkUnmapMemory(device, uniformBuffersMemory[currentImage]);
// }

// void HelloVK::onOrientationChange() {
//   recreateSwapChain();
//   orientationChanged = false;
// }

// void HelloVK::recordCommandBuffer(VkCommandBuffer commandBuffer,
//                                   uint32_t imageIndex) {
//   VkCommandBufferBeginInfo beginInfo{};
//   beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
//   beginInfo.flags = 0;
//   beginInfo.pInheritanceInfo = nullptr;

//   VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

//   VkRenderPassBeginInfo renderPassInfo{};
//   renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
//   renderPassInfo.renderPass = renderPass;
//   renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
//   renderPassInfo.renderArea.offset = {0, 0};
//   renderPassInfo.renderArea.extent = swapChainExtent;

//   VkViewport viewport{};
//   viewport.width = (float)swapChainExtent.width;
//   viewport.height = (float)swapChainExtent.height;
//   viewport.minDepth = 0.0f;
//   viewport.maxDepth = 1.0f;
//   vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

//   VkRect2D scissor{};
//   scissor.extent = swapChainExtent;
//   vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

//   VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};

//   renderPassInfo.clearValueCount = 1;
//   renderPassInfo.pClearValues = &clearColor;
//   vkCmdBeginRenderPass(commandBuffer, &renderPassInfo,
//                        VK_SUBPASS_CONTENTS_INLINE);
//   vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
//                     graphicsPipeline);
//   vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
//                           pipelineLayout, 0, 1, &descriptorSets[currentFrame],
//                           0, nullptr);

//   vkCmdDraw(commandBuffer, 3, 1, 0, 0);
//   vkCmdEndRenderPass(commandBuffer);
//   VK_CHECK(vkEndCommandBuffer(commandBuffer));
// }

void HelloVK::cleanupSwapChain() {
  for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
    vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
  }

  for (size_t i = 0; i < swapChainImageViews.size(); i++) {
    vkDestroyImageView(device, swapChainImageViews[i], nullptr);
  }

  vkDestroySwapchainKHR(device, swapChain, nullptr);
}

void HelloVK::cleanup() {
  vkDeviceWaitIdle(device);
  cleanupSwapChain();
  vkDestroyDescriptorPool(device, descriptorPool, nullptr);

  vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroyBuffer(device, uniformBuffers[i], nullptr);
    vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
  }
  vkDestroyBuffer(device, stagingBuffer, nullptr);
  vkFreeMemory(device, stagingMemory, nullptr);
  vkFreeMemory(device, textureImageMemory, nullptr);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
    vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
    vkDestroyFence(device, inFlightFences[i], nullptr);
  }
  vkDestroyCommandPool(device, commandPool, nullptr);
  vkDestroyPipeline(device, graphicsPipeline, nullptr);
  vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
  vkDestroyRenderPass(device, renderPass, nullptr);
  vkDestroyDevice(device, nullptr);
  if (enableValidationLayers) {
    DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
  }
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vkDestroyInstance(instance, nullptr);
  initialized = false;
}

void HelloVK::setupDebugMessenger() {
  if (!enableValidationLayers) {
    return;
  }

  VkDebugUtilsMessengerCreateInfoEXT createInfo{};
  populateDebugMessengerCreateInfo(createInfo);

  VK_CHECK(CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr,
                                        &debugMessenger));
}

bool HelloVK::checkValidationLayerSupport() {
  uint32_t layerCount;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  for (const char *layerName : validationLayers) {
    bool layerFound = false;
    for (const auto &layerProperties : availableLayers) {
      if (strcmp(layerName, layerProperties.layerName) == 0) {
        layerFound = true;
        break;
      }
    }

    if (!layerFound) {
      return false;
    }
  }
  return true;
}

std::vector<const char *> HelloVK::getRequiredExtensions(
    bool enableValidationLayers) {
  std::vector<const char *> extensions;
  extensions.push_back("VK_KHR_surface");
  extensions.push_back("VK_KHR_android_surface");
  if (enableValidationLayers) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }
  return extensions;
}

void HelloVK::createInstance() {
  assert(!enableValidationLayers ||
         checkValidationLayerSupport());  // validation layers requested, but
                                          // not available!
  auto requiredExtensions = getRequiredExtensions(enableValidationLayers);

  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "Hello Triangle";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "No Engine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  // appInfo.apiVersion = VK_API_VERSION_1_0;
  appInfo.apiVersion = VK_API_VERSION_1_1;

  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;
  createInfo.enabledExtensionCount = (uint32_t)requiredExtensions.size();
  createInfo.ppEnabledExtensionNames = requiredExtensions.data();
  createInfo.pApplicationInfo = &appInfo;

  if (enableValidationLayers) {
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    createInfo.enabledLayerCount =
        static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
    populateDebugMessengerCreateInfo(debugCreateInfo);
    createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
  } else {
    createInfo.enabledLayerCount = 0;
    createInfo.pNext = nullptr;
  }
  VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));

  uint32_t extensionCount = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
  std::vector<VkExtensionProperties> extensions(extensionCount);
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,
                                         extensions.data());
  LOGI("available extensions");
  for (const auto &extension : extensions) {
    LOGI("\t %s", extension.extensionName);
  }
}

/*
 * createSurface can only be called after the android ecosystem has had the
 * chance to provide a native window. This happens after the APP_CMD_START event
 * has had a chance to be called.
 *
 * Notice the window.get() call which is only valid after window has been set to
 * a non null value
 */
void HelloVK::createSurface() {
  assert(window != nullptr);  // window not initialized
  const VkAndroidSurfaceCreateInfoKHR create_info{
      .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
      .pNext = nullptr,
      .flags = 0,
      .window = window.get()};

  VK_CHECK(vkCreateAndroidSurfaceKHR(instance, &create_info,
                                     nullptr /* pAllocator */, &surface));
}

// BEGIN DEVICE SUITABILITY
// Functions to find a suitable physical device to execute Vulkan commands.

QueueFamilyIndices HelloVK::findQueueFamilies(VkPhysicalDevice device) {
  QueueFamilyIndices indices;

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                           queueFamilies.data());

  int i = 0;
  for (const auto &queueFamily : queueFamilies) {
    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphicsFamily = i;
    }

    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
    if (presentSupport) {
      indices.presentFamily = i;
    }

    if (indices.isComplete()) {
      break;
    }

    i++;
  }
  return indices;
}

bool HelloVK::checkDeviceExtensionSupport(VkPhysicalDevice device) {
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       nullptr);

  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       availableExtensions.data());

  std::set<std::string> requiredExtensions(deviceExtensions.begin(),
                                           deviceExtensions.end());

  for (const auto &extension : availableExtensions) {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}

SwapChainSupportDetails HelloVK::querySwapChainSupport(
    VkPhysicalDevice device) {
  SwapChainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                            &details.capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                         details.formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
                                            nullptr);

  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, surface, &presentModeCount, details.presentModes.data());
  }
  return details;
}

bool HelloVK::isDeviceSuitable(VkPhysicalDevice device) {
  QueueFamilyIndices indices = findQueueFamilies(device);
  bool extensionsSupported = checkDeviceExtensionSupport(device);
  bool swapChainAdequate = false;
  if (extensionsSupported) {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
    swapChainAdequate = !swapChainSupport.formats.empty() &&
                        !swapChainSupport.presentModes.empty();
  }
  return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

void HelloVK::pickPhysicalDevice() {
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

  assert(deviceCount > 0);  // failed to find GPUs with Vulkan support!

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

  for (const auto &device : devices) {
    if (isDeviceSuitable(device)) {
      physicalDevice = device;
      break;
    }
  }

  assert(physicalDevice != VK_NULL_HANDLE);  // failed to find a suitable GPU!
}
// END DEVICE SUITABILITY

void HelloVK::createLogicalDeviceAndQueue() {
  QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                            indices.presentFamily.value()};
  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }

  VkPhysicalDeviceFeatures deviceFeatures{};

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.queueCreateInfoCount =
      static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.pEnabledFeatures = &deviceFeatures;
  createInfo.enabledExtensionCount =
      static_cast<uint32_t>(deviceExtensions.size());
  createInfo.ppEnabledExtensionNames = deviceExtensions.data();
  if (enableValidationLayers) {
    createInfo.enabledLayerCount =
        static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
  } else {
    createInfo.enabledLayerCount = 0;
  }

  VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device));

  vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
  vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

VkExtent2D HelloVK::chooseSwapExtent(
    const VkSurfaceCapabilitiesKHR &capabilities) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    int32_t width = ANativeWindow_getWidth(window.get());
    int32_t height = ANativeWindow_getHeight(window.get());
    VkExtent2D actualExtent = {static_cast<uint32_t>(width),
                               static_cast<uint32_t>(height)};

    actualExtent.width =
        std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                   capabilities.maxImageExtent.width);
    actualExtent.height =
        std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);
    return actualExtent;
  }
}

void HelloVK::establishDisplaySizeIdentity() {
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                            &capabilities);

  uint32_t width = capabilities.currentExtent.width;
  uint32_t height = capabilities.currentExtent.height;
  if (capabilities.currentTransform & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR ||
      capabilities.currentTransform & VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR) {
    // Swap to get identity width and height
    capabilities.currentExtent.height = width;
    capabilities.currentExtent.width = height;
  }

  displaySizeIdentity = capabilities.currentExtent;
}

void HelloVK::createSwapChain() {
  SwapChainSupportDetails swapChainSupport =
      querySwapChainSupport(physicalDevice);

  auto chooseSwapSurfaceFormat =
      [](const std::vector<VkSurfaceFormatKHR> &availableFormats) {
        for (const auto &availableFormat : availableFormats) {
          if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
              availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
          }
        }
        return availableFormats[0];
      };

  VkSurfaceFormatKHR surfaceFormat =
      chooseSwapSurfaceFormat(swapChainSupport.formats);

  // Please check
  // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPresentModeKHR.html
  // for a discourse on different present modes.
  //
  // VK_PRESENT_MODE_FIFO_KHR = Hard Vsync
  // This is always supported on Android phones
  VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

  uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
  if (swapChainSupport.capabilities.maxImageCount > 0 &&
      imageCount > swapChainSupport.capabilities.maxImageCount) {
    imageCount = swapChainSupport.capabilities.maxImageCount;
  }
  pretransformFlag = swapChainSupport.capabilities.currentTransform;

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = surface;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = displaySizeIdentity;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  createInfo.preTransform = pretransformFlag;

  QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
  uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                   indices.presentFamily.value()};

  if (indices.graphicsFamily != indices.presentFamily) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
  }
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = VK_NULL_HANDLE;

  VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain));

  vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
  swapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(device, swapChain, &imageCount,
                          swapChainImages.data());

  swapChainImageFormat = surfaceFormat.format;
  swapChainExtent = displaySizeIdentity;
}

void HelloVK::createSyncObjects() {
  imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                               &imageAvailableSemaphores[i]));

    VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                               &renderFinishedSemaphores[i]));

    VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]));
  }
}

}  // namespace vkt