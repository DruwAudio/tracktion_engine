namespace tracktion_engine
{
    AbletonLinkTransport::AbletonLinkTransport()
    {
        initLink();
    }
    AbletonLinkTransport::~AbletonLinkTransport()
    {

    }


    void AbletonLinkTransport::prepareToPlay(double newsampleRate, int newblockSize, double inOutLatencyMs)
    {
        sampleRate = newsampleRate;
        numSamples = newblockSize;
        out_latency_us =  std::chrono::microseconds{ std::llround(1.0e3 * inOutLatencyMs) };

    }

    void AbletonLinkTransport::update()
    {
        calculate_output_time(sampleRate, numSamples );

        // Extract info from link and modify its state as per user requests.
        const auto engine_data = pull_engine_data();
        process_session_state(engine_data);

        // did we just start a new bar?
        auto wrapIndex = getBarPhaseWrapIndex(sampleRate, engine_data.quantum, numSamples);
        if (wrapIndex > -1)
            DBG("LINK: New Bar ");

        sample_time += numSamples  ;
    }

    void AbletonLinkTransport::initLink()
    {

        shared_engine_data = EngineData{ 0., false, false, 1., false };
        lock_free_engine_data = EngineData{ shared_engine_data };

        link.reset(new ableton::Link{ currentBpm });
        link->setTempoCallback([this](const double newBpm) { currentBpm = newBpm; });
        link->enable(true);

    }


    void AbletonLinkTransport::calculate_output_time(const double sample_rate, const int buffer_size)
    {
        // Synchronize host time to reference the point when its output reaches the speaker.
        const auto host_time =  host_time_filter.sampleTimeToHostTime(sample_time);
        output_time = out_latency_us  + host_time;
    }

    std::chrono::microseconds AbletonLinkTransport::calculateTimeAtSample(const std::uint64_t sampleTime, const double sample_rate, const int buffer_size)
    {
        const auto host_time =  host_time_filter.sampleTimeToHostTime(sampleTime);
        const auto output_latency = std::chrono::microseconds{ std::llround(1.0e6 * buffer_size / sample_rate) };
        return output_latency + host_time;
    }


    AbletonLinkTransport::EngineData AbletonLinkTransport::pull_engine_data()
    {   // Safely operate on data isolated from user changes.
        auto engine_data = EngineData{};
        if (engine_data_guard.try_lock()) {
            engine_data.requested_bpm = shared_engine_data.requested_bpm;
            shared_engine_data.requested_bpm = 0;

            engine_data.request_start = shared_engine_data.request_start;
            shared_engine_data.request_start = false;

            engine_data.request_stop = shared_engine_data.request_stop;
            shared_engine_data.request_stop = false;

            lock_free_engine_data.quantum = shared_engine_data.quantum;
            lock_free_engine_data.startstop_sync = shared_engine_data.startstop_sync;

            engine_data_guard.unlock();
        }
        else
            DBG("entry failed");
        engine_data.quantum = lock_free_engine_data.quantum;
        return engine_data;
    }


    void AbletonLinkTransport::process_session_state(const EngineData& engine_data)
    {
        session = std::make_unique<ableton::Link::SessionState>(link->captureAudioSessionState());

        if (engine_data.request_start)
            session->setIsPlaying(true, output_time);

        if (engine_data.request_stop)
            session->setIsPlaying(false, output_time);

        if (!is_playing && session->isPlaying()) {   // Reset the timeline so that beat 0 corresponds to the time when transport starts
            session->requestBeatAtTime(0., output_time, engine_data.quantum);
            is_playing = true;
        }
        else if (is_playing && !session->isPlaying())
            is_playing = false;

        if (engine_data.requested_bpm > 0) // Set the newly requested tempo from the beginning of this buffer.
            session->setTempo(engine_data.requested_bpm, output_time);

        link->commitAudioSessionState(*session); // Timeline modifications are complete, commit the results
    }




    int AbletonLinkTransport::getBarPhaseWrapIndex(const double sample_rate, const double quantum, const int buffer_size)
    {   // Taken from Ableton's linkhut example found on their github.
        const auto micros_per_sample = 1.0e6 / sample_rate;
        for (int i = 0; i < buffer_size; ++i) {
            // Compute the host time for this sample and the last.
            const auto host_time = output_time + std::chrono::microseconds(llround(i * micros_per_sample));
            const auto prev_host_time = host_time - std::chrono::microseconds(llround(micros_per_sample));

            // Only make sound for positive beat magnitudes. Negative beat
            // magnitudes are count-in beats.
            if (session->beatAtTime(host_time, quantum) >= 0.) {

                // If the phase wraps around between the last sample and the
                // current one with respect to a 1 beat quantum, then a sample trigger
                // should occur.
                if (session->phaseAtTime(host_time, beat_length)
                        < session->phaseAtTime(prev_host_time, beat_length))
                {
                    return i;
                }
            }
        }

        return -1;
    }

}// namespace tracktion_engine



