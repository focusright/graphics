#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include "Experiment.h"

class TriangleExperiment {
public:
    void Initialize(ID3D12Device* device);
    void Update(double seconds, const ExperimentParams& params);
    void Record(ID3D12GraphicsCommandList* commands) const;
    void Reset();
    void Shutdown();

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipeline;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertices;
    D3D12_VERTEX_BUFFER_VIEW m_vertexView{};
    float m_angle = 0.0f;
};
