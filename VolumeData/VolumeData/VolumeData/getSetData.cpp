#include <vector>
#include <fstream>
#include <iostream>
#include <string>

#include "datagenerator.hpp"
#include "rapidxml.hpp"
#include "json.hpp"
#include "getSetData.hpp"

using json = nlohmann::json;

/**
* @brief helper function for the switch in getXMLConfig
* @param str the string to convert into an int
* @param h
* @return an converted string as an integer
*/
constexpr unsigned int str2int(const char* str, int h = 0)
{
	return !str[h] ? 5381 : (str2int(str, h + 1) * 33) ^ str[h];
}

// ??TODO: add user-defined output file name
bool GetSetData::saveRawData(std::vector<float> &volData, const DataConfig &cfg)
{
	std::ofstream raw("volData.raw", std::ios::out | std::ios::binary);

	//std::ofstream out(&testFile);
	switch (cfg.precision)
	{
	case 1:
		for (auto voxel : volData)
		{
			raw << static_cast<unsigned char>(voxel * 255.f);
		}
		break;
	case 2:
		for (auto voxel : volData)
		{											
			raw << static_cast<unsigned short>(voxel * 65535.f); // * USHRT_MAX = 65535(.f)
		}
		break;
	case 3: break;
	case 4:
		for (auto voxel : volData)
		{
			raw << static_cast<double>(voxel); // * USHRT_MAX = 65535(.f)
		}
		break;
	default:
		for (auto voxel : volData)
		{
			raw << static_cast<unsigned char>(voxel);
		}
	}

	raw.close();

	// save a .bin file
	std::ofstream dat("volData.dat", std::ios::out | std::ios::binary);

	dat << "ObjectFilename: \t" << "volData.raw" << std::endl;
	dat << "Resolution: \t\t" << cfg.res.x << " " << cfg.res.y << " " << cfg.res.z << std::endl;
	dat << "SliceThickness: \t" << "1.0 1.0 1.0" << std::endl;
	dat << "Data Precision: \t";
	switch (cfg.precision) {
		case 1: dat << "UCHAR" << std::endl; break;
		case 2: dat << "USHRT" << std::endl;; break;
		default: dat << "UNKNOWN" << std::endl; break;
	}

	dat.close();

	return true;
}

bool GetSetData::getXMLConfig(const std::string &file, DataConfig &cfg, std::vector<DataConfig> &configs) {
	
	std::ifstream config(file);
	rapidxml::xml_document<> doc;

	std::vector<char> buffer((std::istreambuf_iterator<char>(config)), std::istreambuf_iterator<char>());

	buffer.push_back('\0');

	doc.parse<0>(&buffer[0]);

	rapidxml::xml_node<> *node = doc.first_node()->first_node();

	for (rapidxml::xml_node<> *outer = node; outer; outer =  outer->next_sibling()) {
		for (rapidxml::xml_node<> *sib = outer->first_node(); sib; sib = sib->next_sibling()) {

			//assert(sib->first_attribute()->value() == NULL && sib->value() == NULL && "The config in your xml contains a nullptr.");

			// case sensitive
			switch (str2int(sib->name())) {
			case str2int("Precision"): cfg.precision = std::stoi(sib->value()); break;

			case str2int("Resolution"):cfg.res = Vec3<int>(std::stoi(sib->first_attribute()->value()),
				std::stoi(sib->first_attribute()->next_attribute()->value()),
				std::stoi(sib->last_attribute()->value())); break;

			case str2int("Coverage"): cfg.coverage = Vec3<double>(std::stod(sib->first_attribute()->value()),
				std::stod(sib->first_attribute()->next_attribute()->value()),
				std::stod(sib->last_attribute()->value())); break;

			case str2int("Shape"): if (strcmp(sib->value(), "cube") == 0) cfg.shape = cube;
				if (strcmp(sib->value(), "sphere") == 0) cfg.shape = sphere; break;

			case str2int("numBodies"): cfg.numBodies = std::stoll(sib->value()); break;

			case str2int("dimBodies"): cfg.dimBodies = Vec3<int>(std::stoi(sib->first_attribute()->value()),
				std::stoi(sib->first_attribute()->next_attribute()->value()),
				std::stoi(sib->last_attribute()->value())); break;

			case str2int("randomBodyLayout"): if (strcmp(sib->value(), "false") == 0) cfg.randomBodyLayout = 0;
				if (strcmp(sib->value(), "true") == 0) cfg.randomBodyLayout = 1; break;

			case str2int("Frequency"): cfg.frequency = Vec3<int>(std::stoi(sib->first_attribute()->value()),
				std::stoi(sib->first_attribute()->next_attribute()->value()),
				std::stoi(sib->last_attribute()->value())); break;

			case str2int("Magnitude"): cfg.magnitude = Vec3<double>(std::stod(sib->first_attribute()->value()),
				std::stod(sib->first_attribute()->next_attribute()->value()),
				std::stod(sib->last_attribute()->value())); break;

			default: std::cout << "XML name do not match the config name."; break;
			}
		}
		configs.push_back(cfg);
	}
	cfg = configs[0];
	config.close();

	return true;
}

bool GetSetData::getJSONConfig(const std::string &file, DataConfig &cfg, std::vector<DataConfig> &configs) {
	std::ifstream inFile(file, std::ios::in, std::ifstream::binary);
	json config;
	inFile >> config;

	try {
		/*
		* get all given config out of the file
		* temporary store it in &cfg and then store it finally in &configs
		*/
		for (auto c : config) {
			// case sensitive
			cfg.precision = c["Precision"];
			cfg.res = Vec3<int>(c["Resolution"][0], c["Resolution"][1], c["Resolution"][2]);
			cfg.coverage = Vec3<double>(c["Coverage"][0], c["Coverage"][1], c["Coverage"][2]);

			std::string s = c["Shape"];
			if (strcmp(s.c_str(), "cube") == 0) cfg.shape = cube;
			if (strcmp(s.c_str(), "sphere") == 0) cfg.shape = sphere;

			cfg.numBodies = static_cast<long long>(c["numBodies"]);
			cfg.dimBodies = Vec3<int>(c["dimBodies"][0], c["dimBodies"][1], c["dimBodies"][2]);
			cfg.randomBodyLayout = c["randomBodyLayout"];
			cfg.frequency = Vec3<int>(c["Frequency"][0], c["Frequency"][1], c["Frequency"][2]);
			cfg.magnitude = Vec3<double>(c["Magnitude"][0], c["Magnitude"][1], c["Magnitude"][2]);

			configs.push_back(cfg);

		}
		cfg = configs[0];
	}
	catch (const std::exception &e) {
		std::cerr << "some failure accured." << std::endl;
		inFile.close();
		return false;
	}
	inFile.close();

	return true;
}