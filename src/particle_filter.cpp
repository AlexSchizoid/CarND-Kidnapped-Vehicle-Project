/*
 * particle_filter.cpp
 *
 *  Created on: Dec 12, 2016
 *      Author: Tiffany Huang
 */

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <math.h> 
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>

#include "particle_filter.h"

using namespace std;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
    // TODO: Set the number of particles. Initialize all particles to first position (based on estimates of 
    //   x, y, theta and their uncertainties from GPS) and all weights to 1. 
    // Add random Gaussian noise to each particle.
    // NOTE: Consult particle_filter.h for more information about this method (and others in this file).
    num_particles = 100;
    std::default_random_engine gen;
    std::normal_distribution<double> N_x(x, std[0]);
    std::normal_distribution<double> N_y(y, std[1]);
    std::normal_distribution<double> N_theta(theta, std[2]);
    
    for(int particleIdx = 0; particleIdx < num_particles; ++particleIdx)
    {
        Particle particle;
        particle.id = particleIdx;
        particle.x = N_x(gen);
        particle.y = N_y(gen);
        particle.theta = N_theta(gen);
        particle.weight = 1;
        
        particles.push_back(particle);
        weights.push_back(1);
    }
    is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
    // TODO: Add measurements to each particle and add random Gaussian noise.
    // NOTE: When adding noise you may find std::normal_distribution and std::default_random_engine useful.
    //  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
    //  http://www.cplusplus.com/reference/random/default_random_engine/
    std::default_random_engine gen;
    
    for(auto &particle : particles)
    {
        double new_x;
        double new_y;
        double new_theta;
        if(yaw_rate == 0)
        {
            new_x = particle.x + velocity * delta_t * cos(particle.theta);
            new_y = particle.y + velocity * delta_t * sin(particle.theta);
            new_theta = particle.theta;
        }
        else
        {
            new_x = particle.x + velocity / yaw_rate * (sin(particle.theta + yaw_rate * delta_t) - sin(particle.theta));
            new_y = particle.y + velocity / yaw_rate * (cos(particle.theta) - cos(particle.theta + yaw_rate * delta_t));
            new_theta = particle.theta + yaw_rate * delta_t;
        }
        normal_distribution<double> N_x(new_x, std_pos[0]);
        normal_distribution<double> N_y(new_y, std_pos[1]);
        normal_distribution<double> N_theta(new_theta, std_pos[2]);
        particle.x = N_x(gen);
        particle.y = N_y(gen);
        particle.theta = N_theta(gen);
    }
}

void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations) {
    // TODO: Find the predicted measurement that is closest to each observed measurement and assign the 
    //   observed measurement to this particular landmark.
    // NOTE: this method will NOT be called by the grading code. But you will probably find it useful to 
    //   implement this method and use it as a helper during the updateWeights phase.

}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
        std::vector<LandmarkObs> observations, Map map_landmarks) {
    // TODO: Update the weights of each particle using a mult-variate Gaussian distribution. You can read
    //   more about this distribution here: https://en.wikipedia.org/wiki/Multivariate_normal_distribution
    // NOTE: The observations are given in the VEHICLE'S coordinate system. Your particles are located
    //   according to the MAP'S coordinate system. You will need to transform between the two systems.
    //   Keep in mind that this transformation requires both rotation AND translation (but no scaling).
    //   The following is a good resource for the theory:
    //   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
    //   and the following is a good resource for the actual equation to implement (look at equation 
    //   3.33
    //   http://planning.cs.uiuc.edu/node99.html
    vector<int> associations;
    vector<double> sense_x;
    vector<double> sense_y;
    int iterationIdx = 0;
    
    for (auto &particle : particles)
    {
        vector<LandmarkObs>  trans_observations;
        
        for(const auto &observation : observations)
        {
            // vehicle-to-map space transformation for all observations
            LandmarkObs trans_obs;
            trans_obs.x = particle.x + observation.x * cos(particle.theta) - observation.y * sin(particle.theta);
            trans_obs.y = particle.y + observation.x * sin(particle.theta) + observation.y * cos(particle.theta);
            trans_observations.push_back(trans_obs);
        }
        
        particle.weight = 1.0;
	vector<Map::single_landmark_s> predictions;

	//we only care about landmarks within the sensor range of the vehicle
	for(const auto &landmark : map_landmarks.landmark_list)
	{
                double distance_to_landmark = dist(particle.x, particle.y, landmark.x_f, landmark.y_f);
		if (distance_to_landmark < sensor_range)
		{
			predictions.push_back(landmark);
		}	
	}

        for(const auto &observation : trans_observations)
        {
            double min_err = sensor_range;
            int association = -1;
	    //for each observation try to match it with one of the map landmarks in sensor range
	    //pick the nearest neighbor
            for (int predictionIdx = 0; predictionIdx < predictions.size(); ++predictionIdx)
            {
                double err = dist(observation.x, observation.y, predictions[predictionIdx].x_f, predictions[predictionIdx].y_f);
                if (err < min_err)
                {
                    min_err = err;
                    association = predictionIdx;
                }                       
            }
            //calculate the particle weight multiplier for the current observation
            if(association != -1)
            {
                double meas_x = observation.x;
                double meas_y = observation.y;
                double mu_x = predictions[association].x_f;
                double mu_y = predictions[association].y_f;
                long double gauss_norm = 1 / (2 * M_PI * std_landmark[0] * std_landmark[1]);
                long double exponent = std::pow(meas_x - mu_x, 2) / (2 * std::pow(std_landmark[0], 2)) + std::pow(meas_y - mu_y, 2) / (2 * std::pow(std_landmark[1],2));
                long double weight_multiplier = gauss_norm * std::exp(-exponent);
                if (weight_multiplier > 0)
                {
                    particle.weight *= weight_multiplier;
                }
            }
            associations.push_back(predictions[association].id_i);
            sense_x.push_back(observation.x);
            sense_y.push_back(observation.y);
        
        }
        particle = SetAssociations(particle, associations, sense_x, sense_y);
	//save weight in a vector in order to simplify resample
        weights[iterationIdx++] = particle.weight;
    }
}

void ParticleFilter::resample() {
    // TODO: Resample particles with replacement with probability proportional to their weight. 
    // NOTE: You may find std::discrete_distribution helpful here.
    //   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
    std::default_random_engine gen;
    discrete_distribution<int> distribution(weights.begin(), weights.end());
    
    vector<Particle> resample_particles;
    
    for(int i = 0; i < num_particles; ++i)
    {
        resample_particles.push_back(particles[distribution(gen)]);
    }
    
    particles = resample_particles;
}
Particle ParticleFilter::SetAssociations(Particle particle, std::vector<int> associations, std::vector<double> sense_x, std::vector<double> sense_y)

{
    //particle: the particle to assign each listed association, and association's (x,y) world coordinates mapping to
    // associations: The landmark id that goes along with each listed association
    // sense_x: the associations x mapping already converted to world coordinates
    // sense_y: the associations y mapping already c?onverted to world coordinates

    //Clear the previous associations
    particle.associations.clear();
    particle.sense_x.clear();
    particle.sense_y.clear();

    particle.associations= associations;
    particle.sense_x = sense_x;
    particle.sense_y = sense_y;

    return particle;
}

string ParticleFilter::getAssociations(Particle best)
{
    vector<int> v = best.associations;
    stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<int>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseX(Particle best)
{
    vector<double> v = best.sense_x;
    stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseY(Particle best)
{
    vector<double> v = best.sense_y;
    stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
