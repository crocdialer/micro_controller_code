#pragma once
#include <Arduino.h>

class WaveSimulation
{
public:
    static constexpr float s_epsilon = 0.001f;

    WaveSimulation(uint32_t max_num_waves = 3);
    ~WaveSimulation();

    //! run the simulation with the_delta_time increment
    void update(uint32_t the_delta_time);

    //! emit a new wave
    void emit_wave(float the_start_intesity = 1.f, float the_start_pos = 0.f);

    //! return the overall intensity at a given location
    float intensity_at_position(float the_position);

    //! return the total length of simulation track in meters
    float track_legth() const { return m_track_length; }
    void set_track_length(float the_track_length){ m_track_length = the_track_length; }

    //! return the absolute propagation speed for simulated waves in meters per second
    float propagation_speed() const;
    void set_propagation_speed(float the_propagation_speed);

    //! return the time in seconds for a wave to decline totally
    float decay_secs() const { return m_decay_secs; }
    void set_decay_secs(float the_decay_secs){ m_decay_secs = the_decay_secs; }

private:
    float m_track_length;
    float m_decay_secs;
    float m_global_propagation_speed;
    float* m_positions;
    float* m_intensities;
    float* m_propagation_speed;
    uint32_t m_max_num_waves;
};
