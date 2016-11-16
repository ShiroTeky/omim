#include "drape_frontend/traffic_renderer.hpp"
#include "drape_frontend/color_constants.hpp"
#include "drape_frontend/visual_params.hpp"

#include "drape/glsl_func.hpp"
#include "drape/shader_def.hpp"
#include "drape/vertex_array_buffer.hpp"

#include "indexer/map_style_reader.hpp"
#include "indexer/scales.hpp"

#include "base/logging.hpp"

#include "std/algorithm.hpp"

namespace df
{

namespace
{

int const kMinVisibleZoomLevel = 10;
int const kMinVisibleArrowZoomLevel = 16;

float const kTrafficArrowAspect = 24.0f / 8.0f;

float const kLeftWidthInPixel[] =
{
  // 1   2     3     4     5     6     7     8     9     10
  0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
  //11   12     13    14    15    16    17    18    19     20
  0.75f, 0.75f, 1.0f, 2.0f, 2.0f, 3.0f, 3.0f, 4.0f, 5.0f, 8.0f
};

float const kRightWidthInPixel[] =
{
  // 1   2     3     4     5     6     7     8     9     10
  2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 4.0f, 4.0f,
  //11  12    13    14    15    16    17    18    19     20
  4.0f, 4.0f, 4.0f, 2.0f, 2.0f, 3.0f, 3.0f, 4.0f, 5.0f, 8.0f
};

float CalculateHalfWidth(ScreenBase const & screen, bool left)
{
  double const zoomLevel = GetZoomLevel(screen.GetScale());
  double zoom = trunc(zoomLevel);
  int const index = zoom - 1.0;
  float const lerpCoef = zoomLevel - zoom;

  float const * halfWidth = left ? kLeftWidthInPixel : kRightWidthInPixel;
  float radius = 0.0f;
  if (index < scales::UPPER_STYLE_SCALE)
    radius = halfWidth[index] + lerpCoef * (halfWidth[index + 1] - halfWidth[index]);
  else
    radius = halfWidth[scales::UPPER_STYLE_SCALE];

  return radius * VisualParams::Instance().GetVisualScale();
}

} // namespace

void TrafficRenderer::AddRenderData(ref_ptr<dp::GpuProgramManager> mng,
                                    TrafficRenderData && renderData)
{
  m_renderData.emplace_back(move(renderData));

  ref_ptr<dp::GpuProgram> program = mng->GetProgram(m_renderData.back().m_state.GetProgramIndex());
  program->Bind();
  m_renderData.back().m_bucket->GetBuffer()->Build(program);

  for (size_t j = 0; j < m_renderData.back().m_bucket->GetOverlayHandlesCount(); j++)
  {
    TrafficHandle * handle = static_cast<TrafficHandle *>(m_renderData.back().m_bucket->GetOverlayHandle(j).get());
    m_handles.insert(make_pair(handle->GetSegmentId(), handle));
  }
}

void TrafficRenderer::UpdateTraffic(TrafficSegmentsColoring const & trafficColoring)
{
  for (auto const & segment : trafficColoring)
  {
    auto it = m_texCoords.find(static_cast<size_t>(segment.m_speedGroup));
    if (it == m_texCoords.end())
      continue;

    auto handleIt = m_handles.find(segment.m_id);
    if (handleIt != m_handles.end())
      handleIt->second->SetTexCoord(it->second);
  }
}

void TrafficRenderer::RenderTraffic(ScreenBase const & screen, int zoomLevel,
                                    ref_ptr<dp::GpuProgramManager> mng,
                                    dp::UniformValuesStorage const & commonUniforms)
{
  if (m_renderData.empty() || zoomLevel < kMinVisibleZoomLevel)
    return;

  float const pixelHalfWidth = CalculateHalfWidth(screen, true /* left */);
  float const invPixelLength = 1.0f / (2.0f * pixelHalfWidth * kTrafficArrowAspect);

  GLFunctions::glClearDepth();
  for (TrafficRenderData & renderData : m_renderData)
  {
    ref_ptr<dp::GpuProgram> program = mng->GetProgram(renderData.m_state.GetProgramIndex());
    program->Bind();
    dp::ApplyState(renderData.m_state, program);

    dp::UniformValuesStorage uniforms = commonUniforms;
    math::Matrix<float, 4, 4> const mv = renderData.m_tileKey.GetTileBasedModelView(screen);
    uniforms.SetMatrix4x4Value("modelView", mv.m_data);
    uniforms.SetFloatValue("u_opacity", 1.0f);
    uniforms.SetFloatValue("u_trafficParams", pixelHalfWidth, CalculateHalfWidth(screen, false /* left */),
                           invPixelLength, zoomLevel >= kMinVisibleArrowZoomLevel ? 1.0f : 0.0f);
    dp::ApplyUniforms(uniforms, program);

    renderData.m_bucket->Render(renderData.m_state.GetDrawAsLine());
  }
}

void TrafficRenderer::SetTexCoords(TrafficTexCoords && texCoords)
{
  m_texCoords = move(texCoords);
}

void TrafficRenderer::ClearGLDependentResources()
{
  m_renderData.clear();
  m_handles.clear();
  m_texCoords.clear();
}

void TrafficRenderer::Clear(MwmSet::MwmId const & mwmId)
{
  for (size_t i = 0; i < m_renderData.size();)
  {
    if (m_renderData[i].m_mwmId == mwmId)
    {
      swap(m_renderData[i], m_renderData.back());
      m_renderData.pop_back();
    }
    else
    {
      ++i;
    }
  }
}

} // namespace df

