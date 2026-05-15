#include <cassert>
#include <iostream>

#include "service/observability/observability_service.h"

int main() {
    ddz::ObservabilityService obs;
    obs.Configure(true, 1000);

    ddz::ObservabilityService::SetCurrentTraceId("trace-1");
    assert(ddz::ObservabilityService::CurrentTraceId() == "trace-1");

    obs.Record("login", ddz::ErrorCode::OK, 10);
    obs.Record("login", ddz::ErrorCode::INVALID_TOKEN, 30);
    obs.Record("match", ddz::ErrorCode::OK, 20);

    auto snap = obs.TryBuildMetricsSnapshot(500);
    assert(snap.has_value());
    assert(snap->find("event=metrics_snapshot") != std::string::npos);
    assert(snap->find("login.total=2") != std::string::npos);
    assert(snap->find("match.total=1") != std::string::npos);

    auto none = obs.TryBuildMetricsSnapshot(1200);
    assert(!none.has_value());

    std::cout << "test_p5_observability_service passed" << std::endl;
    return 0;
}
