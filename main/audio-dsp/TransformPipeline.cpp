#include "bell/dsp/TransformPipeline.h"

// Transform types
#include "bell/dsp/BiquadComboTransform.h"
#include "bell/dsp/BiquadTransform.h"
#include "bell/dsp/GainTransform.h"

#ifndef BELL_DISABLE_TAOJSON
// Used for JSON deserialization of the transforms
#include <tao/json.hpp>
#include <tao/json/contrib/traits.hpp>
#endif

#include "bell/Logger.h"

using namespace bell::dsp;

void Transform::setChannels(const std::vector<uint8_t>& channels) {
  std::scoped_lock lock(accessMutex);
  this->channels = channels;
}

void TransformPipeline::addTransform(
    const std::shared_ptr<Transform>& transform) {
  std::scoped_lock lock(accessMutex);
  transforms.push_back(transform);
}

void TransformPipeline::process(DataSlots& sampleSlots) {
  std::scoped_lock lock(accessMutex);

  // Process the samples through each transform in the pipeline
  for (const auto& transform : transforms) {
    transform->process(sampleSlots);
  }
}

#ifndef BELL_DISABLE_TAOJSON
namespace {
std::shared_ptr<Transform> parseTransform(const tao::json::value& json) {
  if (!json.is_object()) {
    return nullptr;
  }

  if (!json.find("type")) {
    return nullptr;
  }

  const auto& transformType = json.at("type").get_string_type();

  std::vector<uint8_t> channels;

  if (json.find("channels") != nullptr && json.at("channels").is_array()) {
    // Parse the channels
    channels = json.at("channels").as<std::vector<uint8_t>>();
  } else if (json.find("channel") != nullptr &&
             json.at("channel").is_number()) {
    // Parse the channel
    channels.push_back(json.at("channel").as<uint8_t>());
  } else {
    throw std::invalid_argument("Channels not specified for transform");
  }

  // Parse the gain transform
  if (transformType == "gain") {
    auto gain = std::make_shared<GainTransform>();

    // Assign channels
    gain->setChannels(channels);

    if (json.find("gain") != nullptr && json.at("gain").is_number()) {
      gain->configure(json.at("gain").as<float>());
    } else {
      throw std::invalid_argument("Gain not specified for gain transform");
    }
    return gain;
  }

  if (transformType == "biquad") {
    if (channels.size() > 1) {
      throw std::invalid_argument("Biquad transform only supports one channel");
    }

    if (!json.find("biquad_type")) {
      throw std::invalid_argument(
          "Biquad type not specified for biquad transform");
    }

    auto biquad = std::make_shared<BiquadTransform>();
    biquad->setChannels(channels);

    const auto& biquadType = json.at("biquad_type").get_string();

    // Try to fetch the optional parameters
    std::optional<float> f = json.optional<float>("freq");
    std::optional<float> q = json.optional<float>("q");
    std::optional<float> gain = json.optional<float>("gain");
    std::optional<float> slope = json.optional<float>("slope");
    std::optional<float> bandwidth = json.optional<float>("bandwidth");

    biquad->configure(BiquadTransform::stringToType(biquadType), f, q, gain,
                      slope, bandwidth);
    return biquad;
  }

  if (transformType == "biquad_combo") {
    if (channels.size() > 1) {
      throw std::invalid_argument("Biquad transform only supports one channel");
    }

    if (!json.find("combo_type")) {
      throw std::invalid_argument(
          "Biquad type not specified for biquad combo transform");
    }

    auto combo = std::make_shared<BiquadComboTransform>();
    combo->setChannels(channels);

    const auto& comboType = json.at("combo_type").get_string();

    if (!json.find("freq")->is_number()) {
      throw std::invalid_argument("Frequency not specified for biquad combo");
    }

    if (!json.find("order")->is_number()) {
      throw std::invalid_argument("Order not specified for biquad combo");
    }

    combo->configure(BiquadComboTransform::stringToType(comboType),
                     json.at("freq").as<float>(), json.at("order").as<int>());

    return combo;
  }

  throw std::invalid_argument("Invalid transform type");
  return nullptr;
}
}  // namespace

std::shared_ptr<TransformPipeline> TransformPipeline::fromJson(
    const tao::json::value& json) {
  auto pipeline = std::make_shared<TransformPipeline>();
  for (const auto& transformJson : json.get_array()) {
    pipeline->addTransform(parseTransform(transformJson));
  }

  BELL_LOG(info, "dsp_engine", "Pipeline created with {} transforms",
           pipeline->transforms.size());

  return pipeline;
}
#endif