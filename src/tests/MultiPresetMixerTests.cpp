#include "dsp/MultiPresetMixer.h"
#include "presets/PresetTypes.h"
#include "resources/ResourceLibrary.h"
#include <cassert>
#include <cmath>
#include <vector>
#include <iostream>

using namespace guitarfx;

static Preset MakePassthroughPreset(const std::string& id)
{
  Preset preset;
  preset.id = id;
  preset.name = id;

  GraphNode in;
  in.id = "in";
  in.type = kNodeTypeInput;

  GraphNode out;
  out.id = "out";
  out.type = kNodeTypeOutput;

  GraphEdge e;
  e.from = in.id;
  e.to = out.id;

  preset.graph.nodes = { in, out };
  preset.graph.edges = { e };
  return preset;
}

int main()
{
  MultiPresetMixer mixer;
  ResourceLibrary lib;
  mixer.SetResourceLibrary(&lib);

  const double sr = 48000.0;
  const int bs = 64;
  mixer.Prepare(sr, bs);

  // Add two passthrough presets
  auto pL = MakePassthroughPreset("pL");
  auto pR = MakePassthroughPreset("pR");
  bool ok1 = mixer.AddActivePreset(pL, "pL", "LeftPreset");
  bool ok2 = mixer.AddActivePreset(pR, "pR", "RightPreset");
  assert(ok1 && ok2);

  // Configure pan and mix
  mixer.SetPresetPan("pL", -1.0); // hard left
  mixer.SetPresetPan("pR", +1.0); // hard right
  mixer.SetPresetMix("pL", 0.5);
  mixer.SetPresetMix("pR", 0.5);

  // Prepare input: unity on both channels
  std::vector<float> inL(static_cast<size_t>(bs), 1.0f);
  std::vector<float> inR(static_cast<size_t>(bs), 1.0f);
  std::vector<float> outL(static_cast<size_t>(bs), 0.0f);
  std::vector<float> outR(static_cast<size_t>(bs), 0.0f);

  float* inputs[2] = { inL.data(), inR.data() };
  float* outputs[2] = { outL.data(), outR.data() };

  mixer.Process(inputs, outputs, bs);

  // Expect 0.5 on both channels (0.5 left from pL + 0.5 right from pR)
  for (int i = 0; i < bs; ++i)
  {
    if (std::fabs(outL[static_cast<size_t>(i)] - 0.5f) > 1e-4f ||
        std::fabs(outR[static_cast<size_t>(i)] - 0.5f) > 1e-4f)
    {
      std::cerr << "Mismatch at sample " << i << ": L=" << outL[static_cast<size_t>(i)]
                << " R=" << outR[static_cast<size_t>(i)] << std::endl;
      return 1;
    }
  }

  std::cout << "MultiPresetMixer pan/mix test passed" << std::endl;
  return 0;
}
