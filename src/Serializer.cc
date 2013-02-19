#include "Serializer.h"

#include "PDG.h"
#include "CtrlDep.h"
#include "json/json.h"

#include <cstdio>
#include <cstring>
#include <utility>
#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/DenseMap.h>

using namespace chopper;
using namespace llvm;
using std::pair;
using std::make_pair;

SerializerException::SerializerException() noexcept
{}

SerializerException::SerializerException(const string &msg) noexcept:
    exception(), message(msg) {}
SerializerException::SerializerException(const char *msg) noexcept:
    exception(), message(msg) {}

const char*
SerializerException::what() const noexcept
{
    return message.c_str();
}

Serializer::Serializer() 
{
}

Serializer::~Serializer()
{
}

static int
serializer_printer_cb(void *userdata, const char *s, uint32_t length)
{
    //errs() << s << "[" << length << "]\n";

    fwrite(s, sizeof(char), length, (FILE*)userdata);
    return 0;
}

void
Serializer::serialize(SerialInfo &sInfo)
{
    json_printer print;
    FILE *fp = fopen(sInfo.filename.c_str(), "w+");
    if (!fp) {
        throw new SerializerException("file open");
    }
    if (json_print_init(&print, serializer_printer_cb, 
                fp)) {
        throw new SerializerException();
    }


    /* index all instruction and basicblock info */
    DenseMap<Instruction*, pair<size_t, size_t> > instMap;
    DenseMap<BasicBlock*, pair<size_t, size_t> >
        bbMap;
    size_t bbId = 0;
    size_t instId = 0;
    PDG *pdg = sInfo.pdg;
    CDG *cdg = sInfo.cdg;
    for (BasicBlock &bb : sInfo.func->getBasicBlockList()) {
        for (Instruction &inst : bb) {
            instMap[&inst] = make_pair(instId, 0);
            instId ++;
        }
        bbMap[&bb] = make_pair(bbId, 0);
        bbId ++;
    }
    bbId = 0;
    for (CDG::iterator iter=cdg->begin(); iter!=cdg->end();
            iter++, bbId++) {
        if (!iter->bb) continue;
        bbMap[iter->bb].second = bbId;
    }

    instId = 0;
    json_print_args(&print, json_print_pretty,
            JSON_OBJECT_BEGIN, JSON_KEY, "pdg", 3,
            JSON_ARRAY_BEGIN, -1);
    // prcocess pdg 
    for (PDG::iterator iter=pdg->begin(); iter!=pdg->end();
            iter++, instId++) {
        string buffer;
        if (!iter->inst) continue;
        instMap[iter->inst].second = instId;
        llvm::raw_string_ostream ss(buffer);
        ss << *(iter->inst) ;

        // parent block id 
        BasicBlock *parent = iter->inst->getParent();

        assert (parent && bbMap.count(parent));
        string pid = std::to_string(bbMap[parent].first);
        string realInstId = std::to_string(instMap[iter->inst].first);

        json_print_args(&print, json_print_raw,
            JSON_OBJECT_BEGIN, 
                JSON_KEY, "inst", 4,
                JSON_STRING, buffer.c_str(), buffer.length(),
                JSON_KEY, "bbId", 4,
                JSON_INT, pid.c_str(), pid.length(),
                JSON_KEY, "realId", 6,
                JSON_INT, realInstId.c_str(), realInstId.length(),
                JSON_KEY, "deps", 4,
                JSON_ARRAY_BEGIN, -1);
        PDG::AdjList adjList = iter->adjList;
        for (PDG::AdjList::iterator adjIter= adjList.begin();
                adjIter != adjList.end(); adjIter++) {

            string id = std::to_string(adjIter->id);
            json_print_args(&print, json_print_raw,
                JSON_OBJECT_BEGIN, 
                    JSON_KEY, "id", 2,
                    JSON_INT, id.c_str(), id.length(),
                    //TODO type
                JSON_OBJECT_END, -1);

        }
        json_print_args(&print, json_print_raw,
                JSON_ARRAY_END, JSON_OBJECT_END, -1);
    }

    json_print_args(&print, json_print_pretty,
            JSON_ARRAY_END, JSON_KEY, "cdg", 3,
            JSON_ARRAY_BEGIN, -1);

    //process cdg
    for (CDG::iterator iter=cdg->begin(); iter!=cdg->end();
            iter++) {
        if (!iter->bb) continue;
        bbMap[iter->bb].second = bbId;
        string bbName = iter->bb->getName().str();
        json_print_args(&print, json_print_raw,
            JSON_OBJECT_BEGIN, 
                JSON_KEY, "name", 4,
                JSON_STRING, bbName.c_str(), bbName.length(), -1);
        //TODO add instruction ID if needed
        Instruction *termInst = iter->bb->getTerminator();
        if (termInst && instMap.count(termInst)) {
            string termId = std::to_string(instMap[termInst].first);
            json_print_args(&print, json_print_raw,
                JSON_KEY, "termInst", 8,
                JSON_STRING, termId.c_str(), termId.length(), -1);
        }
        json_print_args(&print, json_print_raw,
             JSON_OBJECT_END, -1);
    }

    json_print_args(&print, json_print_raw,
            JSON_ARRAY_END, JSON_OBJECT_END, -1);

    fclose(fp);
    json_print_free(&print);

}

