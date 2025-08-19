#ifndef STREAMING_MANAGER_H
#define STREAMING_MANAGER_H

#include <memory>

// Forward declarations to avoid circular dependencies
class V4L2Device;
class DmaBuffersManager;

class StreamingManager {
public:
    StreamingManager(V4L2Device& device, DmaBuffersManager& output_buffers);
    ~StreamingManager();

    [[nodiscard]] bool start();
    [[nodiscard]] bool stop();
    [[nodiscard]] bool is_active() const;
    void set_inactive();


private:
    [[nodiscard]] bool queueOutputBuffers();
    [[nodiscard]] bool queueOutputBuffer(unsigned int index);
    [[nodiscard]] bool enableStreaming();

    V4L2Device& device_;
    DmaBuffersManager& output_buffers_;
    
    enum class State {
        STOPPED,
        STARTING,
        ACTIVE,
        STOPPING,
        ERROR
    };
    State state_ = State::STOPPED;
};

#endif // STREAMING_MANAGER_H
