#pragma once
#include <stdio.h>
#include <vector>
#include <string>

#define ARG_MAX_ARG_NAME_LENGTH 256
#define ARG_MAX_DESCRIPTION_LENGTH 256
#define ARG_MAX_OUT_LENGTH 2048
#define ARG_MAX_LENGTH 2048

// Append more here if needed
#define ARG_BOOL 0
#define ARG_STRING 1
#define ARG_INT 2

class ArgOption {
private:
	char shortArg[ARG_MAX_ARG_NAME_LENGTH] = { 0 };
	char longArg[ARG_MAX_ARG_NAME_LENGTH] = { 0 };
	char description[ARG_MAX_DESCRIPTION_LENGTH] = { 0 };
	int type = 0;
	char* carrierArg = nullptr;
	void* out = nullptr;
	bool* bDefault = nullptr;
	int* iDefault = nullptr;
	char* sDefault = nullptr;
	bool required = false;


	char value[ARG_MAX_OUT_LENGTH] = { 0 };
public:
	bool isProvided = false;

	ArgOption(int type, const char s[ARG_MAX_ARG_NAME_LENGTH], const char l[ARG_MAX_ARG_NAME_LENGTH], const char d[ARG_MAX_DESCRIPTION_LENGTH], bool required) {
		strcpy_s((char*)this->shortArg, ARG_MAX_ARG_NAME_LENGTH, s);
		strcpy_s((char*)this->longArg, ARG_MAX_ARG_NAME_LENGTH, l);
		strcpy_s((char*)this->description, ARG_MAX_DESCRIPTION_LENGTH, d);
		this->type = type;
		this->required = required;
	}

	void setOut(bool* out) {
		this->out = out;
		this->type = ARG_BOOL;
	}
	void setOut(char* out) {
		this->out = out;
		this->type = ARG_STRING;
	}
	void setOut(int* out) {
		this->out = out;
		this->type = ARG_INT;
	}
	void setDefault(bool* def) {
		this->bDefault = def;
		this->type = ARG_BOOL;
		*((bool*)this->out) = *def;
	}
	void setDefault(char* def) {
		this->sDefault = def;
		this->type = ARG_STRING;
		strncpy_s((char*)this->out, ARG_MAX_OUT_LENGTH, def, strlen(def));
	}
	void setDefault(int* def) {
		this->iDefault = def;
		this->type = ARG_INT;
		*((int*)this->out) = *def;
	}
	void setValue(char* value) {
		this->isProvided = true;
		strncpy_s(this->value, ARG_MAX_OUT_LENGTH, value, strlen(value));
		switch (this->type) {
		case ARG_BOOL: {
			bool* out = (bool*)this->out;
			if (strlen(this->value) == 0) {
				// No value for bool given, using true if name is set otherwise false. If a default is given, if name is set, it gets inverted
				if (this->bDefault == nullptr) {
					// No default is set
					*out = true;
				}
				else {
					*out = !*this->bDefault;
				}
				break;
			}
			*out = strncmp(this->value, "true", sizeof("true")) == 0;// TODO extend
			break;
		};
		case ARG_STRING: {
			char* out = (char*)this->out;
			strncpy_s(out, ARG_MAX_OUT_LENGTH, this->value, strlen(this->value));
			break;
		}
		case ARG_INT: {
			int* out = (int*)this->out;
			*out = atoi(this->value);
			break;
		}
		}
	}

	char* getValue() {
		return this->value;
	}


	bool isRequired() {
		return this->required;
	}

	bool compareName(char* name) {
		bool isLongName = strncmp(name, this->longArg, strlen(this->longArg)) == 0;
		bool isShortName = strncmp(name, this->shortArg, strlen(this->shortArg)) == 0;
		return isLongName || isShortName;
	}

	char* getShortName() {
		return this->shortArg;
	}

	char* getLongName() {
		return this->longArg;
	}

	char* getDescription() {
		return this->description;
	}
};




// Parses os args using this pattern `--lname=value` or `-sname=value`
class ArgParser
{
private:
	std::vector<ArgOption*> args;
	char** argv = nullptr;
	int argc = 0;
	char* generalDescription = { 0 };
	std::vector<char*> noOptionArgs;
public:
	ArgParser(int argc, char** argv, const char* generalDescription) noexcept {
		this->argv = argv;
		this->argc = argc;
		this->generalDescription = (char*)generalDescription;
	}
	void addArg(ArgOption* arg) noexcept {
		this->args.push_back(arg);
	}

	std::vector<char*> getNonOptionArgs() {
		return this->noOptionArgs;
	}

	void printUsage() {
		printf("%s\n", this->generalDescription);
		for (ArgOption* argOption : this->args) {
			printf("\t-%s, --%s # %s\n", argOption->getShortName(), argOption->getLongName(), argOption->getDescription());
		}
		if (this->args.size() > 0) {
			printf("\nArgs: -k=value, --key=value or -k/--key for boolean values\n");
		}
	}
	static bool parseOne(char* osArg, char* nameOut, char* valueOut) {

		if (osArg == nullptr || strlen(osArg) == 0) return false;

		if (strlen(osArg) > ARG_MAX_LENGTH) {
			throw std::exception("os argument is too long. please increase ARG_MAX_LENGTH");
		}

		// Cloning for immutability
		char* clone = (char*)malloc(ARG_MAX_LENGTH);
		int osArgLength = strlen(osArg);
		strcpy_s(clone, ARG_MAX_LENGTH, osArg);


		// Checking name prefix
		bool isLong = true;
		if (strncmp(clone, "-", 1) == 0) {
			isLong = false;
		}
		else if (strncmp(clone, "--", 2) == 0) {
			isLong = true;
		}
		else {
			return false;
		}

		char* name = clone;


		if (isLong) {
			strncpy_s(nameOut, ARG_MAX_ARG_NAME_LENGTH, name + 2, strlen(name + 2) >= ARG_MAX_ARG_NAME_LENGTH ? ARG_MAX_ARG_NAME_LENGTH - 1 : strlen(name + 2));
		}
		else {
			int len = strlen(name + 1) >= ARG_MAX_ARG_NAME_LENGTH ? ARG_MAX_ARG_NAME_LENGTH - 1 : strlen(name + 1);
			strncpy_s(nameOut, ARG_MAX_LENGTH, name + 1, len);
		}

		// Splitting
		char* delimeter = strchr(clone, '=');
		if (delimeter == nullptr) {
			return true;
		}
		*delimeter = '\00'; // Creating two null terminated strings name\00value\00
		char* value = delimeter + 1;

		strncpy_s(valueOut, ARG_MAX_OUT_LENGTH, value, strlen(value));
		return true;
	}

	bool parseAll() {

		for (int i = 0; i < this->argc; i++) {
			char* arg = argv[i];
			char nameOut[ARG_MAX_LENGTH] = { 0 };
			char valueOut[ARG_MAX_OUT_LENGTH] = { 0 };

			bool isOption = ArgParser::parseOne(arg, nameOut, valueOut);

			if (!isOption) {
				this->noOptionArgs.push_back(arg);
				continue;
			}

			if (strlen(nameOut) == 0) {
				char exceptionStr[1024] = { 0 };
				sprintf_s(exceptionStr, "name is zero length (arg: '%s')", arg);
				throw std::exception(exceptionStr);
			}

			for (ArgOption* argOption : this->args) {
				if (!argOption->compareName(nameOut)) { continue; }
				argOption->setValue(valueOut);
				break;
			}
		}

		// Check for required but not supplied values
		for (ArgOption* argOption : this->args) {
			if (!argOption->isProvided && argOption->isRequired()) {
				char exceptionStr[1024] = { 0 };
				sprintf_s(exceptionStr, "arg %s (-%s) is required but not given", argOption->getLongName(), argOption->getShortName());
				throw std::exception(exceptionStr);
			}
		}

		return true;
	}
};

