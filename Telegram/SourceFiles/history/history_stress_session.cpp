#include "session.h"

#include "model.h"

#include <onnxruntime_cxx_api.h>

std::unique_ptr<Ort::Session> session;

namespace {

struct Runtime {
	Ort::Env environment{ ORT_LOGGING_LEVEL_ERROR, "TelegramStress" };
	~Runtime();
};

Runtime::~Runtime() {
	session.reset();
}

} // namespace

void init_session() {
	// Static ONNX internals must outlive both the session and its environment.
	static auto runtime = Runtime();
	auto options = Ort::SessionOptions();
	options.SetExecutionMode(ORT_SEQUENTIAL);
	options.SetInterOpNumThreads(1);
	options.SetIntraOpNumThreads(1);
	options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);
	session = std::make_unique<Ort::Session>(
		runtime.environment,
		model_onnx,
		model_onnx_len,
		options);
}
