#include "utils.h"
#include "WaveSimulation.h"

WaveSimulation::WaveSimulation(uint32_t max_num_waves):
m_track_length(12.f),
m_decay_secs(2.f),
m_global_propagation_speed(15.f)
{
    m_max_num_waves = max_num_waves;
    m_positions = new float[max_num_waves];
    m_intensities = new float[max_num_waves];
    m_propagation_speed = new float[max_num_waves];
    memset(m_positions, 0, sizeof(float) * max_num_waves);
    memset(m_intensities, 0, sizeof(float) * max_num_waves);
    set_propagation_speed(m_global_propagation_speed);
}

WaveSimulation::~WaveSimulation()
{
    delete[](m_positions);
    delete[](m_intensities);
    delete[](m_propagation_speed);
}

void WaveSimulation::update(uint32_t the_delta_time)
{
    float delta_secs = the_delta_time / 1000.f;

    for(uint32_t i = 0; i < m_max_num_waves; ++i)
    {
        if(m_intensities[i] >= s_epsilon)
        {
            float new_pos = m_positions[i] + m_propagation_speed[i] * delta_secs;

            // end of track?
            if(new_pos >= 0.f && new_pos <= m_track_length){ m_positions[i] = new_pos; }
            else
            {
                m_positions[i] = new_pos < 0.f ? -new_pos : 2 * m_track_length - new_pos;
                m_propagation_speed[i] *= -1.f;
            }
            m_intensities[i] -= 1.f / m_decay_secs * delta_secs;
        }
        else{ m_intensities[i] = 0.f; }
    }
}

void WaveSimulation::emit_wave(float the_start_intesity, float the_start_pos)
{
    // find unused slot
    for(uint32_t i = 0; i < m_max_num_waves; ++i)
    {
        if(m_intensities[i] < s_epsilon)
        {
            // found an unused slot
            m_intensities[i] = the_start_intesity;
            m_positions[i] = the_start_pos;
            m_propagation_speed[i] = m_global_propagation_speed;
            break;
        }
    }
}

float WaveSimulation::intensity_at_position(float the_position)
{
    float sum = 0.f;
    float quad_factor = 18.f;

    for(uint32_t i = 0; i < m_max_num_waves; i++)
    {
        if(m_intensities[i] >= s_epsilon)
        {
            float distance = m_positions[i] - the_position;
            sum += m_intensities[i] / (1.f + quad_factor * distance * distance);
        }
    }
    return sum;
}

float WaveSimulation::propagation_speed() const
{
     return m_global_propagation_speed;
}
void WaveSimulation::set_propagation_speed(float the_propagation_speed)
{
    m_global_propagation_speed = the_propagation_speed;

    for(uint32_t i = 0; i < m_max_num_waves; i++)
    {
        m_propagation_speed[i] = the_propagation_speed * sgn(m_propagation_speed[i]);
    }
}
