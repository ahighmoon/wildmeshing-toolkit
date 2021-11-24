//
// Created by Yixin Hu on 10/12/21.
//

#pragma once
#include "common.h"

namespace tetwild
{
	class Envelope
	{ // shall we make this virtual too as we have multiple options?
	  // DP: I would like to get rid of the sampling, why are we keeping it?
		double get_point_dist(const Vector3f &p);
		double get_segment_dist(const Vector3f &p1, const Vector3f &p2);
		double get_triangle_dist(const Vector3f &p1, const Vector3f &p2, const Vector3f &p3);

		void sample_a_segment( // why sampling? this should be the new one?
							   // input:
			const Vector3f &p1, const Vector3f &p2,
			// output:
			std::vector<Vector3f> &ps);

		void sample_a_triangle(
			// input:
			const Vector3f &p1, const Vector3f &p2, const Vector3f &p3,
			// output:
			std::vector<Vector3f> &ps);
	};
} // namespace tetwild
