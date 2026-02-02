#pragma once

#include "presets/PresetTypes.h"
#include <nlohmann/json.hpp>

namespace guitarfx
{
  inline nlohmann::json SerializeResourceRef(const ResourceRef& ref)
  {
    nlohmann::json json;
    if (!ref.resourceType.empty())
      json["resourceType"] = ref.resourceType;
    if (!ref.resourceId.empty())
      json["resourceId"] = ref.resourceId;
    if (!ref.filePath.empty())
      json["filePath"] = ref.filePath.string();
    if (!ref.embeddedId.empty())
      json["embeddedId"] = ref.embeddedId;
    if (!ref.parameterId.empty())
      json["parameterId"] = ref.parameterId;
    if (ref.parameterValue.has_value())
      json["parameterValue"] = *ref.parameterValue;
    if (!ref.parameters.empty())
    {
      json["parameters"] = nlohmann::json::object();
      for (const auto& [key, value] : ref.parameters)
      {
        json["parameters"][key] = value;
      }
    }
    return json;
  }

  inline ResourceRef DeserializeResourceRef(const nlohmann::json& json)
  {
    ResourceRef ref;
    ref.resourceType = json.value("resourceType", json.value("type", ""));
    ref.resourceId = json.value("resourceId", json.value("id", ""));
    ref.filePath = json.value("filePath", "");
    ref.embeddedId = json.value("embeddedId", "");
    ref.parameterId = json.value("parameterId", "");
    if (json.contains("parameterValue") && json["parameterValue"].is_number())
    {
      ref.parameterValue = json["parameterValue"].get<double>();
    }
    if (json.contains("parameters") && json["parameters"].is_object())
    {
      for (const auto& [key, value] : json["parameters"].items())
      {
        if (value.is_number())
        {
          ref.parameters[key] = value.get<double>();
        }
      }
    }
    return ref;
  }

  inline nlohmann::json SerializeNode(const GraphNode& node)
  {
    nlohmann::json json;
    json["id"] = node.id;
    json["type"] = node.type;
    if (!node.category.empty())
      json["category"] = node.category;
    if (!node.label.empty())
      json["label"] = node.label;
    if (!node.enabled)
      json["enabled"] = node.enabled;

    if (!node.params.empty())
    {
      json["params"] = nlohmann::json::object();
      for (const auto& [key, value] : node.params)
      {
        json["params"][key] = value;
      }
    }

    if (!node.config.empty())
    {
      json["config"] = nlohmann::json::object();
      for (const auto& [key, value] : node.config)
      {
        json["config"][key] = value;
      }
    }

    if (!node.resources.empty())
    {
      json["resources"] = nlohmann::json::array();
      for (const auto& res : node.resources)
      {
        if (res.IsValid())
        {
          json["resources"].push_back(SerializeResourceRef(res));
        }
      }
    }

    return json;
  }

  inline GraphNode DeserializeNode(const nlohmann::json& json)
  {
    GraphNode node;
    node.id = json.value("id", "");
    node.type = json.value("type", "");
    node.category = json.value("category", "");
    node.label = json.value("label", json.value("displayName", ""));

    if (json.contains("enabled"))
    {
      node.enabled = json.value("enabled", true);
    }
    else if (json.contains("bypassed"))
    {
      node.enabled = !json.value("bypassed", false);
    }
    else
    {
      node.enabled = true;
    }

    if (json.contains("params") && json["params"].is_object())
    {
      for (const auto& [key, value] : json["params"].items())
      {
        if (value.is_number())
        {
          node.params[key] = value.get<double>();
        }
      }
    }

    if (json.contains("config") && json["config"].is_object())
    {
      for (const auto& [key, value] : json["config"].items())
      {
        if (value.is_string())
        {
          node.config[key] = value.get<std::string>();
        }
      }
    }

    if (json.contains("resources") && json["resources"].is_array())
    {
      for (const auto& resJson : json["resources"])
      {
        if (resJson.is_object())
        {
          node.resources.push_back(DeserializeResourceRef(resJson));
        }
      }
    }
    else if (json.contains("resource") && json["resource"].is_object())
    {
      node.resources.push_back(DeserializeResourceRef(json["resource"]));
    }

    return node;
  }

  inline nlohmann::json SerializeEdge(const GraphEdge& edge)
  {
    nlohmann::json json;
    json["from"] = edge.from;
    json["to"] = edge.to;
    if (edge.fromPort != 0)
      json["fromPort"] = edge.fromPort;
    if (edge.toPort != 0)
      json["toPort"] = edge.toPort;
    if (edge.gain != 1.0)
      json["gain"] = edge.gain;
    return json;
  }

  inline GraphEdge DeserializeEdge(const nlohmann::json& json)
  {
    GraphEdge edge;
    edge.from = json.value("from", "");
    edge.to = json.value("to", "");
    edge.fromPort = json.value("fromPort", 0);
    edge.toPort = json.value("toPort", 0);
    edge.gain = json.value("gain", 1.0);
    return edge;
  }

  inline nlohmann::json SerializeSignalGraph(const SignalGraph& graph)
  {
    nlohmann::json json;
    json["nodes"] = nlohmann::json::array();
    json["edges"] = nlohmann::json::array();

    for (const auto& node : graph.nodes)
    {
      json["nodes"].push_back(SerializeNode(node));
    }

    for (const auto& edge : graph.edges)
    {
      json["edges"].push_back(SerializeEdge(edge));
    }

    return json;
  }

  inline SignalGraph DeserializeSignalGraph(const nlohmann::json& json)
  {
    SignalGraph graph;
    if (json.contains("nodes") && json["nodes"].is_array())
    {
      for (const auto& nodeJson : json["nodes"])
      {
        if (nodeJson.is_object())
        {
          graph.nodes.push_back(DeserializeNode(nodeJson));
        }
      }
    }

    if (json.contains("edges") && json["edges"].is_array())
    {
      for (const auto& edgeJson : json["edges"])
      {
        if (edgeJson.is_object())
        {
          graph.edges.push_back(DeserializeEdge(edgeJson));
        }
      }
    }

    return graph;
  }

  inline void to_json(nlohmann::json& j, const GlobalSignalChainConfig& c)
  {
    j = nlohmann::json{
      {"inputGain", c.inputGain},
      {"monoMode", c.monoMode},
      {"inputChannel", c.inputChannel},
      {"autoLevelInput", c.autoLevelInput},
      {"outputGain", c.outputGain},
      {"autoLevelOutput", c.autoLevelOutput},
      {"limiterEnabled", c.limiterEnabled},
      {"preChainGraph", SerializeSignalGraph(c.preChainGraph)},
      {"postChainGraph", SerializeSignalGraph(c.postChainGraph)}
    };
  }

  inline void from_json(const nlohmann::json& j, GlobalSignalChainConfig& c)
  {
    c.inputGain = j.value("inputGain", 0.0);
    c.monoMode = j.value("monoMode", false);
    c.inputChannel = j.value("inputChannel", 0);
    c.autoLevelInput = j.value("autoLevelInput", false);
    c.outputGain = j.value("outputGain", 0.0);
    c.autoLevelOutput = j.value("autoLevelOutput", false);
    c.limiterEnabled = j.value("limiterEnabled", false);

    if (j.contains("preChainGraph") && j["preChainGraph"].is_object())
      c.preChainGraph = DeserializeSignalGraph(j["preChainGraph"]);

    if (j.contains("postChainGraph") && j["postChainGraph"].is_object())
      c.postChainGraph = DeserializeSignalGraph(j["postChainGraph"]);

    if (c.preChainGraph.nodes.empty() && c.preChainGraph.edges.empty())
      c.preChainGraph = GlobalSignalChainConfig::BuildDefaultPreChainGraph();

    if (c.postChainGraph.nodes.empty() && c.postChainGraph.edges.empty())
      c.postChainGraph = GlobalSignalChainConfig::BuildDefaultPostChainGraph();
  }

} // namespace guitarfx
