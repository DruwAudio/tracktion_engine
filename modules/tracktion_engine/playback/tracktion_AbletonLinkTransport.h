#include <ableton/Link.hpp>
#include <ableton/link/HostTimeFilter.hpp>

namespace tracktion_engine {

class AbletonLinkTransport
{
public:
    AbletonLinkTransport();
    ~AbletonLinkTransport();
    double currentBpm = 60;


    void prepareToPlay(double newsampleRate, int newblockSize);
    void update();

    /** Data that's passed around between thread */
    struct EngineData
    {
        double requested_bpm;
        bool request_start;
        bool request_stop;
        double quantum;
        bool startstop_sync;
        JUCE_LEAK_DETECTOR(EngineData)
    };





private :

    double sampleRate = 44100;
    int numSamples = 512;

    void initLink();

    void calculate_output_time(const double sample_rate, const int buffer_size);
    std::chrono::microseconds calculateTimeAtSample(const std::uint64_t sampleTime, const double sample_rate, const int buffer_size);
    EngineData pull_engine_data();
    void process_session_state(const EngineData& engine_data);

    float sampleToTick(double sampleIndex, int ticksPerBeat);

    std::unique_ptr<ableton::Link> link;
    ableton::link::HostTimeFilter<ableton::link::platform::Clock> host_time_filter;
    std::unique_ptr<ableton::Link::SessionState> session;
    static constexpr double beat_length = 1;

    EngineData shared_engine_data, lock_free_engine_data;
    std::mutex engine_data_guard;

    std::chrono::microseconds output_time;
    std::chrono::microseconds out_latency_us;

    std::uint64_t sample_time = 0;
    bool is_playing = false;

    /**
    * Checks for quantum phase wrap.
    Returns the sample index on which the phase wrapped.
    Returns -1 if bar didnt wrap around.
    */
    int getBarPhaseWrapIndex(const double sample_rate, const double quantum, const int buffer_size);



};


} // namespace tracktion_engine
