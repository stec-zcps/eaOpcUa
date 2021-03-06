#ifndef EAOPCUA_SERVER_HPP
#define EAOPCUA_SERVER_HPP

#include <map>
#include <iostream>
#include <unistd.h>
#include <open62541.h>

#include "types.hpp"
#include "EA.hpp"

#include "open62541.h"

//#define PC

static std::pair<std::string, UA_NodeId> addNode(const UA_Server* server, const std::string& name, void* value, int d, const UA_NodeId* parent){
	if(d == -1)
		return {};

	UA_VariableAttributes attr = UA_VariableAttributes_default;

	UA_Variant_setScalar(&attr.value, value, &UA_TYPES[d]);

	attr.dataType = UA_TYPES[d].typeId;
	attr.accessLevel = UA_ACCESSLEVELMASK_READ;
	attr.displayName = UA_LOCALIZEDTEXT((char*)std::string("de-DE").c_str(), (char*)name.c_str());

	UA_NodeId nNodeId = UA_NODEID_STRING(1, (char*)name.c_str());
	UA_QualifiedName nNodeName = UA_QUALIFIEDNAME(1, (char*)name.c_str());
	UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);

	UA_NodeId ret;

	UA_Server_addVariableNode((UA_Server*)server, nNodeId, *parent,
							  parentReferenceNodeId, nNodeName,
							  UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, &ret);

	return {name, ret};
};

static std::pair<std::string, UA_NodeId> addNode(const UA_Server* server, const std::string& name, const address& adr, const UA_NodeId* parent){
	int d = -1;
	void* v = adr.v;

	switch(adr.datatype){
		case address::Datatype::Bit: {
			d = UA_TYPES_BOOLEAN;
			//v = calloc(1, sizeof(UA_Boolean));
#ifndef PC
			auto state = (UA_Boolean) readBit(adr);
			memcpy(v, &state, sizeof(UA_Boolean));
#endif
			break;
		}
		case address::Datatype::Byte: {
			d = UA_TYPES_BYTE;
			//v = calloc(1, sizeof(UA_Byte));
#ifndef PC
			UA_Byte state;
			if(readBytes(adr, &state)){
				memcpy(v, &state, sizeof(UA_Byte));
			}else{
				//free(v);
				return {};
			}
#endif
			break;
		}
		case address::Datatype::Int16: {
			d = UA_TYPES_INT16;
			//v = calloc(1, sizeof(UA_Int16));
#ifndef PC
			UA_Int16 state;
			if(readBytes(adr, 4, (std::uint8_t*) &state)){
				memcpy(v, &state, sizeof(UA_Int16));
			}else{
				//free(v);
				return {};
			}
#endif
			break;
		}
		default: break;
	}

	return addNode(server, name, v, d, parent);
}

static std::pair<std::string, UA_NodeId> addNode(const UA_Server* server, const std::string& name, const operation& o, const UA_NodeId* parent){
	int d = -1;
	void* v = o.v;

	switch(o.Typ){
		case operation::Type::Double: {
			d = UA_TYPES_DOUBLE;
			o.op((operation*)&o);

			break;
		}
		default: break;
	}

	return addNode(server, name, v, d, parent);
}

static void aktNode(const UA_Server* server, int d, void* value, const UA_NodeId* nodeId){
	if(d == -1)
		return;

	UA_Variant var;
	UA_Variant_init(&var);

	UA_Variant_setScalar(&var, value, &UA_TYPES[d]);
	UA_Server_writeValue((UA_Server*)server, *(UA_NodeId*)nodeId, var);
}

static void aktNode(const UA_Server* server, const address& adr, const UA_NodeId* nodeId){
	int d = -1;
	void* v = adr.v;

	switch(adr.datatype){
		case address::Datatype::Bit: {
			d = UA_TYPES_BOOLEAN;
			//v = calloc(1, sizeof(UA_Boolean));
#ifndef PC
			auto state = (UA_Boolean) readBit(adr);
			memcpy(v, &state, sizeof(UA_Boolean));
#endif
			break;
		}
		case address::Datatype::Byte: {
			d = UA_TYPES_BYTE;
			//v = calloc(1, sizeof(UA_Byte));
#ifndef PC
			UA_Byte state;
			if(readBytes(adr, &state)){
				memcpy(v, &state, sizeof(UA_Byte));
			}else{
				//free(v);
				return;
			}
#endif
			break;
		}
		case address::Datatype::Int16: {
			d = UA_TYPES_INT16;
			//v = calloc(1, sizeof(UA_Int16));
#ifndef PC
			UA_Int16 state;
			if(readBytes(adr, 4, (std::uint8_t*) &state)){
				memcpy(v, &state, sizeof(UA_Int16));
			}else{
				//free(v);
				return;
			}
#endif
			break;
		}
		default: break;
	}

	aktNode(server, d, v, nodeId);
}

static void aktNode(const UA_Server* server, const operation& o, const UA_NodeId* nodeId){
	int d = -1;
	void* v = o.v;

	switch(o.Typ){
		case operation::Type::Double: {
			d = UA_TYPES_DOUBLE;
			o.op((operation*)&o);

			break;
		}
		default: break;
	}

	aktNode(server, d, v, nodeId);
}

static void addObjekt(const UA_Server* server, const std::string& name, const UA_NodeId* parent, UA_NodeId* nodeId){
        UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;
        oAttr.displayName = UA_LOCALIZEDTEXT((char*)"de-DE", (char*)name.c_str());
        UA_Server_addObjectNode((UA_Server*)server, UA_NODEID_NULL, *parent, UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                UA_QUALIFIEDNAME(1, (char*)name.c_str()), UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                                oAttr, nullptr, nodeId);
}

unsigned int sleep_time = 50000;

void server(const std::map<std::string, address>& io_addresses, const std::map<std::string, operation>& operations, const bool* lauf){
	int state = 0;

	UA_Server *server = nullptr;
	UA_ServerConfig *config = nullptr;
	std::map<std::string, UA_NodeId> nodeVerzeichnisEA, nodeVerzeichnisOp;

    UA_NodeId objektEA, objectOp;

	while(*lauf){
		switch(state){
			case 0:{
				config = UA_ServerConfig_new_minimal(4840, nullptr);
				server = UA_Server_new(config);

				UA_StatusCode r1 = UA_Server_run_startup(server);

				++state;
				break;
			}
			case 1:{
			    UA_NodeId objFolder = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);

                addObjekt(server, std::string("EA"), &objFolder, &objektEA);
                addObjekt(server, std::string("Generated"), &objFolder, &objectOp);

				for(auto& n : io_addresses){
					std::pair<std::string, UA_NodeId> nNode = addNode(server, n.first, n.second, &objektEA);
					if(!nNode.first.empty()){
						nodeVerzeichnisEA.insert(nNode);
					}
				}
				for(auto& n : operations){
					std::pair<std::string, UA_NodeId> nNode = addNode(server, n.first, n.second, &objectOp);
					if(!nNode.first.empty()){
						nodeVerzeichnisOp.insert(nNode);
					}
				}

				++state;
				break;
			}
			case 2:{
				for(auto& n : nodeVerzeichnisEA){
					aktNode(server, io_addresses.at(n.first), &(n.second));
				}
				for(auto& n : nodeVerzeichnisOp){
					aktNode(server, operations.at(n.first), &(n.second));
				}
			}
			default: break;
		}

		UA_UInt16 r_t = UA_Server_run_iterate(server, true);
		usleep(sleep_time);
	}

	UA_Server_delete(server);
	UA_ServerConfig_delete(config);
}

#endif //EAOPCUA_SERVER_HPP
