/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow/lite/delegates/hexagon/builders/resize_nearest_neighbor_builder.h"

#include <stdint.h>

#include <limits>

#include "tensorflow/lite/c/builtin_op_data.h"
#include "tensorflow/lite/delegates/hexagon/hexagon_nn/hexagon_nn.h"
#include "tensorflow/lite/kernels/kernel_util.h"

namespace tflite {
namespace delegates {
namespace hexagon {
TfLiteStatus ResizeNearestNeighborOpBuilder::PopulateSubGraph(
    const TfLiteIntArray* inputs, const TfLiteIntArray* outputs,
    TfLiteContext* context) {
  // Input data tensor.
  int tensor_id = inputs->data[0];
  const auto& input_tensor = context->tensors[tensor_id];
  AddInput(graph_builder_->GetHexagonTensorId(tensor_id));
  TF_LITE_ENSURE_STATUS(
      ComputeMinAndMaxQuantValues(input_tensor, &input_min_, &input_max_));
  auto* input_min_const = graph_builder_->AddConstNodeWithData(
      kScalarShape, reinterpret_cast<char*>(&input_min_), sizeof(input_min_));
  auto* input_max_const = graph_builder_->AddConstNodeWithData(
      kScalarShape, reinterpret_cast<char*>(&input_max_), sizeof(input_max_));

  // Output dimensions tensor.
  tensor_id = inputs->data[1];
  const auto& output_dim_tensor = context->tensors[tensor_id];
  if (output_dim_tensor.allocation_type == kTfLiteMmapRo) {
    // If the output dimensions input is a constant, bake it into the Hexagon
    // graph as a Const node.
    auto* const_output_dim_node =
        graph_builder_->AddConstNodeWithData(tensor_id, output_dim_tensor);
    AddInput(TensorID(const_output_dim_node->GetID(), 0));
  } else {
    AddInput(graph_builder_->GetHexagonTensorId(tensor_id));
  }

  // Min/max values for input tensor.
  AddInput(TensorID(input_min_const->GetID(), 0));
  AddInput(TensorID(input_max_const->GetID(), 0));

  // Align corners.
  const TfLiteResizeNearestNeighborParams* params =
      reinterpret_cast<const TfLiteResizeNearestNeighborParams*>(builtin_data_);
  align_corners_ = params->align_corners;
  auto* align_corners_const = graph_builder_->AddConstNodeWithData(
      kScalarShape, reinterpret_cast<char*>(&align_corners_),
      sizeof(align_corners_));
  AddInput(TensorID(align_corners_const->GetID(), 0));

  // Hexagon outputs for this node.
  int output_batch_size, output_height_size, output_width_size,
      output_depth_size;
  GetDims(&output_batch_size, &output_height_size, &output_width_size,
          &output_depth_size, context->tensors[outputs->data[0]].dims);
  node_output_ = AddOutput(sizeof(uint8_t), 4,
                           {output_batch_size, output_height_size,
                            output_width_size, output_depth_size});
  AddOutput(sizeof(float), 4, kScalarShape);
  AddOutput(sizeof(float), 4, kScalarShape);

  return kTfLiteOk;
}

TfLiteStatus ResizeNearestNeighborOpBuilder::RegisterOutputs(
    const TfLiteIntArray* outputs, TfLiteContext* context) {
  // Should be only 1 output.
  graph_builder_->AddTensorWithID(outputs->data[0], node_output_.first,
                                  node_output_.second);
  return kTfLiteOk;
}

ResizeNearestNeighborOpBuilder::~ResizeNearestNeighborOpBuilder() {}

OpBuilder* CreateResizeNearestNeighborBuilder(GraphBuilder* graph_builder,
                                              int op_type) {
  return new ResizeNearestNeighborOpBuilder(graph_builder, op_type);
}

}  // namespace hexagon
}  // namespace delegates
}  // namespace tflite
