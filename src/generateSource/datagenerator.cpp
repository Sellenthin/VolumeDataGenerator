#include "datagenerator.hpp"

#include <cstdlib>
#include <cassert>
#include <iostream>
#include <string>
#include <time.h>

#define SQRT_3 1.732050807568877
#define M_2_PI 0.63661977236758134308

/*
* a function pointer used in generateScalarData to select different functions
* from the Frequency class
*/
double (Frequency::*mptr)(const Vec3<int>&, const Vec3<double>&, const Vec3<unsigned int>&, const Vec3<unsigned int>&) = NULL;

DataGenerator::DataGenerator()
{
	// generate constantly new seeds
	std::srand((unsigned)time(NULL));
}

bool DataGenerator::isPrime(unsigned long prime) {
	for (unsigned long i = 2; i <= prime / 2; ++i) {
		if (prime % i == 0) return false;
	}
	return true;
}

double DataGenerator::Halton_Seq(int index, unsigned long base) {
	double f = 1, r = 0;
	while (index > 0) {
		f = f / base;
		r = r + f * (index % base);
		index = index / base;
	}
	return r;
}

bool DataGenerator::isOverlapping(const Vec3<double> &center,
	const Vec3<double> &radius,
	const Shape &shape,
	const std::vector<Vec3<double>> &elementCenters)
{
	if (elementCenters.empty()) {
		std::cout << "There are no elements within elementCenters" << std::endl;
		return false;
	}

	for (auto el : elementCenters) {
		if (((shape == sphere) || (shape == cube)) && (center.dist(el) < 2.0*radius.length())) {
			return true;
		}
	}

	return false;
}


Vec3<double> DataGenerator::addRandomElement(const Vec3<double> &radius)
{
	// creates new random center coordinates as long as they don't match the requirements
	assert(radius < Vec3<double>(1.0) && radius > Vec3<double>(0.0));

	Vec3<double> center(0.0, 0.0, 0.0);
	do {
		center.x = static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX); // RAND_MAX 32767
	} while (center.x - radius.x < 0.0 || center.x + radius.x > 1.0);

	do {
		center.y = static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX);
	} while (center.y - radius.y < 0.0 || center.y + radius.y > 1.0);

	do {
		center.z = static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX);
	} while (center.z - radius.z < 0.0 || center.z + radius.z > 1.0);

	return center;
}

Vec3<double> DataGenerator::addHaltonElement(const Vec3<double> &radius, int baseX, int baseY, int baseZ, long long &index) {

	// radius has to be less or equal than 0.5 and greater than 0.0
	assert(radius <= Vec3<double>(0.5) && radius > Vec3<double>(0.0));
	Vec3<double> center(0.0, 0.0, 0.0);

	bool xBounds, yBounds, zBounds = false;

	do {
		// calculates halton value for the given index and prime base
		center.x = Halton_Seq(index, baseX);
		center.y = Halton_Seq(index, baseY);
		center.z = Halton_Seq(index, baseZ);

		// boundary check for each boundary
		xBounds = center.x - radius.x < 0.0 || center.x + radius.x > 1.0;
		yBounds = center.y - radius.y < 0.0 || center.y + radius.y > 1.0;
		zBounds = center.z - radius.z < 0.0 || center.z + radius.z > 1.0;

		++index;
		/* 
		* every boundary check has to be false
		* so the element is guaranteed within the volume
		*/
	} while (xBounds || yBounds || zBounds);

	return center;
}

std::vector<Vec3<double>> DataGenerator::addElementsStructured(long long n, Vec3<int> dimBodies)
{
	std::vector<Vec3<double>> centers;
	// equidistant interval between each element in same direction
	Vec3<double> spacing = Vec3<double>(1.0) / static_cast<Vec3<double>>(dimBodies);
	
	//iterate over each direction and calculate the corresponding center
	for (int i = 0; i < dimBodies.x; ++i)
	{
		for (int j = 0; j < dimBodies.y; ++j)
		{
			for (int k = 0; k < dimBodies.z; ++k)
			{
				if (centers.size() == (size_t)n)
				{
					return centers;
				}
				// calculates the structured center of an element
				Vec3<double> center(i, j, k);
				center *= spacing;
				center += spacing * 0.5;
				centers.push_back(center);
			}
		}
	}

	assert(centers.size() == (size_t)n);
	return centers;
}

int DataGenerator::generateVolume(DataConfig cfg)
{
	long long volSize = static_cast<long long>(cfg.res.x) * static_cast<long long>(cfg.res.y) *
		static_cast<long long>(cfg.res.z);

	// initialize vectors
	m_isShape.clear();
	m_isShape.resize(volSize, false);

	m_volData.clear();
	m_volData.resize(volSize, 0.0f);

	m_elementCenters.clear();

	// max 1.0 for coverage = 100, dimBodies = 1
	Vec3<double> diameter = (cfg.coverage * 0.01) / static_cast<Vec3<double>>(cfg.dimBodies);

	if (cfg.numBodies >= volSize / 2L) {
		std::cout << "Number of bodies is greater than half the volume resolution: "
			<< cfg.numBodies << " > " << volSize / 2L << std::endl;
		return false;
	}

	// if there is only one element and no randomness, then the element is set in the middle
	if (cfg.numBodies == 1L && cfg.randomBodyLayout == 0) {
		m_elementCenters.push_back(Vec3<double>(0.5, 0.5, 0.5));
	}

	// pseudorandom layout for all elements
	else if (cfg.randomBodyLayout == 1 && cfg.coverage < Vec3<int>(80)) {	
		long long i = 0;

		// as long as there are less elements added than requested
		// try to add more elements
		// if there are as much elements found as requested within 15 seconds
		// return the already found elements and move on from there
		clock_t begin = clock();
		while (i < cfg.numBodies)
		{
			if ((clock() - begin) == 15000.0) {
				std::cout << "no proper center found within 15 seconds" << '\n' << "returning the already found centers" << std::endl;
				break;
			}

			Vec3<double> center = addRandomElement(diameter / 2.0);
			if (center.x == -1.0) break;

			// check, if elements are overlapping
			// if yes, drop current center and try a new one
			if (!isOverlapping(center, diameter / 2.0, cfg.shape, m_elementCenters))
			{
				m_elementCenters.push_back(center);
				++i;
				std::cout << "Added element " << i << "/" << cfg.numBodies << std::endl;
			}
		}
	}

	// layout based on halton sequence
	else if (cfg.randomBodyLayout == 2 && cfg.coverage < Vec3<double>(80.0)) {	
		long long i = 0;

		// skip first 30 elements of the halton sequence for a better result
		long long skip = 30;

		// as long as there are less elements added than requested
		// try to add more elements
		// if there are as much elements found as requested within 15 seconds
		// return the already found elements and move on from there
		clock_t begin = clock();
		while (i < cfg.numBodies)
		{
			if ((clock() - begin) == 15000.0) {
				std::cout << "no proper center found within 15 seconds" << '\n' << "returning the already found centers" << std::endl;
				break;
			}
			// add new elements with prime bases 32257, 56519, 96479
			Vec3<double> center = addHaltonElement(diameter / 2.0, 32257, 56519, 96479, skip);
			if (center.x == -1.0) break;

			// check, if elements are overlapping
			// if yes, drop current center and try a new one
			if (!isOverlapping(center, diameter / 2.0, cfg.shape, m_elementCenters))
			{
				m_elementCenters.push_back(center);
				++i;
				std::cout << "Added element " << i << "/" << cfg.numBodies << std::endl;
			}
		}
	}
	else if (cfg.randomBodyLayout == 3) {
		return true;
	}

	// structured layout
	else {
		m_elementCenters = addElementsStructured(cfg.numBodies, cfg.dimBodies);
	}

	for (auto el : m_elementCenters)
	{
		// map center coordinates from [0, 1] to [0, cfg.res]
		Vec3<int> iCenter = static_cast<Vec3<int>>(el * cfg.res);
		Vec3<int> iRange = static_cast<Vec3<int>>(diameter * 0.5 * cfg.res);

		//        assert(iCenter + iRange < cfg.res);
		//        assert(iCenter - iRange > Vec3<int>(0));

		double sphereRange = iRange.length() / SQRT_3;

		/*
		* goes iRange steps in each direction from center
		* and sets m_isShape = true according to the current shape
		*/
#pragma omp parallel for
		for (int curX = iCenter.x - iRange.x; curX < iCenter.x + iRange.x; ++curX)
		{
			for (int curY = iCenter.y - iRange.y; curY < iCenter.y + iRange.y; ++curY)
			{
				for (int curZ = iCenter.z - iRange.z; curZ < iCenter.z + iRange.z; ++curZ)
				{
					long long id = curX + curY * cfg.res.x + curZ * cfg.res.x * cfg.res.y;

					// centerDist is needed for the sphere to check if (curX, curY, curZ) is within the sphere
					double centerDist = (iCenter - Vec3<int>(curX, curY, curZ)).length();
					if (cfg.shape == cube || (cfg.shape == sphere && centerDist <= sphereRange))
					{
						m_isShape.at(id) = true;
					}
				}
			}
		}
	}
	return true;
}


int DataGenerator::generateScalarData(const DataConfig &cfg)
{
	if (cfg.res.x < 1 || cfg.res.y < 1 || cfg.res.z < 1)
	{
		generateVolume(cfg);
	}

	if (m_volData.size() != static_cast<size_t>(cfg.res.x*cfg.res.y*cfg.res.z))
	{
		m_volData.resize(cfg.res.x*cfg.res.y*cfg.res.z);
	}

	/*
	* set select to get a frequency function
	* mostly used for checking purposes for the user-defined function
	* compare the selected function with the user-defined function
	* and check if they match
	*
	switch (_select) {
		case 1: mptr = &Frequency::sine; break;
		case 2: mptr = &Frequency::quadraticSine; break;
		case 3: mptr = &Frequency::sineExp; break;
		case 4: mptr = &Frequency::cosine; break;
		case 5: mptr = &Frequency::quadraticCosine; break;
		case 6: mptr = &Frequency::cosineExp; break;
		case 7: mptr = &Frequency::rect; break;
		case 8: mptr = &Frequency::triangle; break;
		default: mptr = &Frequency::sine; break;
	}*/

	// generate global frequency
#pragma omp parallel for
	for (int dirX = 0; dirX < cfg.res.x; ++dirX)
	{
		for (unsigned int dirY = 0; dirY < cfg.res.y; ++dirY)
		{
			for (unsigned int dirZ = 0; dirZ < cfg.res.z; ++dirZ)
			{
				long long id = dirX + dirY * cfg.res.x + dirZ * cfg.res.x * cfg.res.y;
				// fill bodies with scalar data of defined frequency

				// used for perlin noise values
				if (cfg.randomBodyLayout == 3) {
					double x = static_cast<double>(dirX) / static_cast<double>(cfg.res.x);
					double y = static_cast<double>(dirY) / static_cast<double>(cfg.res.y);
					double z = static_cast<double>(dirZ) / static_cast<double>(cfg.res.z);
					//double noise = 10.0 * ((_perlin.noise(Vec3<double>(x, y, z)) + 1.0) / 2.0);
					double noise = _perlin.noise(Vec3<double>(x, y, z)) * _freq.sine(Vec3<unsigned int>(dirX, dirY, dirZ), cfg.magnitude, cfg.frequency, cfg.res);
					//noise = noise - floor(noise);
					m_volData.at(id) = noise;
				}
				else if (m_isShape.at(id))
				{		
					// freq.*mptr points to the function selected above
					m_volData.at(id) = _freq.sine(Vec3<unsigned int>(dirX, dirY, dirZ), cfg.magnitude, cfg.frequency, cfg.res);
				}
			}
		}
	}

	return true;
}
