#pragma once
#include <vector>
#include <genetic-algorithm.hpp>
#include <evo-devo-gpu.hpp>
#include <algorithm>
#include <cfloat>
#include <cstring>
#include <random>
#include <iostream>

struct SelectionSubstage{
    enum Type{
        ROULETTE        = 0b000,
        LINEAR          = 0b001,
        EXPONENTIAL     = 0b010,
        TOURNAMENT      = 0b011,
        TWO_POINTS_CO   = 0b100,
        UNIFORM_CO      = 0b101,
        MUTATE          = 0b110
    } type;
    union{
        float selectionPressure;
        float k1;
        int tournamentSize;
        float mutationProbability;
        float desiredGeneticDistance;
    } param;
    int individuals;
    //Explicit constructors seem to be necessary to make vector.emplace_back to work
    SelectionSubstage(Type a, int b, int c){type = a; param.tournamentSize = b; individuals=c;}
    SelectionSubstage(Type a, float b, int c){type = a; param.k1= b; individuals=c;}
};

template<class W>
struct SelectionStage{
    std::vector<SelectionSubstage> substages;
    W weights[2];
    int repeats;
};

template<class W, class T>
struct SelectionPlan{
    SelectionStage<W> *stages;
    int number;
    bool maximizeFitness;
    T targets;
};

template <class W, class T>
void evolve(Genome_t *genomes, int populationSize, int developmentStages, Body *bodies, float *fitness, SelectionPlan<W, T> plan, float fitnessFunction(Body *body, const T &targets, const W &weights), uint64_t geneticDistance(const Genome_t& first, const Genome_t& second)){
    OpenGLHandles handles;
    if(!initializeOpenGLHandles(&handles)){
        exit(EXIT_FAILURE);    
    }
    Genome_t *thisGenome    = genomes;
    Genome_t *nextGenome    = new Genome_t[populationSize];
    uint8_t **thisGen       = new uint8_t*[populationSize];
    for(int i=0;i<populationSize;++i){
        thisGen[i] = new uint8_t[256*256*256];
    }
    uint8_t **nextGen       = new uint8_t*[populationSize];
    for(int i=0;i<populationSize;++i){
        nextGen[i] = new uint8_t[256*256*256];
    }
    float *currentFitness  = fitness;
    float *nextFitness     = new float[populationSize];
    Body dummyBody;
    dummyBody.cells = new Cell[256*256*256];
    int winners[populationSize];
    std::vector<int> invalidatedBodies;
    invalidatedBodies.reserve(populationSize);
    for(int i=0;i<populationSize;++i){
        std::cout<<"Developing genome "<<i<<"...\n";
        generateGenome(thisGenome + i);
        loadGenome(&handles, thisGenome + i);
        developBody(&handles, developmentStages);
        birthBody(thisGen[i]);
        isolateBody(&dummyBody, thisGen[i]);
        currentFitness[i] = fitnessFunction(&dummyBody, plan.targets, plan.stages[0].weights[0]);
    }
    std::cout<<std::endl;
    std::random_device rd;  
    std::mt19937 gen(rd()); 
    uint64_t genesLoci[stemCellsTypes*fieldsNumber*8 + 7*fieldsNumber];
    for(int i=0;i<stemCellsTypes*fieldsNumber*8;++i){
        genesLoci[i] = i;
    }
    for(int i=0;i<7*fieldsNumber;++i){
        genesLoci[stemCellsTypes*fieldsNumber*8 + i] = stemCellsTypes*fieldsNumber*8 +2*i;
    }

    for(int i=0;i<plan.number;++i){
        W previousWeights = plan.stages[i].weights[0];
        for(int j=0;j<plan.stages[i].repeats;++j){
            std::cout<<"Stage "<<i<<", repeat "<<j<<std::endl;
            int individualsGenerated = 0;
            for(auto &substage: plan.stages[i].substages){
                switch(substage.type){
                    case SelectionSubstage::ROULETTE:{
                        rouletteRanking(populationSize, currentFitness, winners+individualsGenerated, substage.individuals);
                        break;
                    }
                    case SelectionSubstage::LINEAR:{
                        linearRanking(populationSize, currentFitness, plan.maximizeFitness, substage.param.selectionPressure, winners+individualsGenerated, substage.individuals);
                        break;
                    }
                    case SelectionSubstage::EXPONENTIAL:{
                        exponentialRanking(populationSize, currentFitness, plan.maximizeFitness, substage.param.k1, winners+individualsGenerated, substage.individuals);
                        break;
                    }
                    case SelectionSubstage::TOURNAMENT:{
                        tournamentRanking(populationSize, currentFitness, plan.maximizeFitness, substage.param.tournamentSize, winners+individualsGenerated, substage.individuals);
                        break;
                    }
                    case SelectionSubstage::TWO_POINTS_CO:{
                        //We select half of the parents according to their fitness with a roulette wheel, 
                        //then they select their partners among the rest of the population according to genetic similarity, aiming for substage.param.desiredGeneticDistance
                        //NOTE the distance is not absolute, but relative to the max distance in the population
                        int parents[substage.individuals];
                        rouletteRanking(populationSize, currentFitness, parents, substage.individuals);
                        uint64_t maxDelta = 0;
                        for(int l=0;l<substage.individuals;++l){
                            for(int m=0;m<populationSize;++m){
                                bool ok = true;
                                for(int o=0;o<substage.individuals;++o){
                                    if(m==parents[o]){
                                        ok = false;
                                        break;
                                    }
                                }
                                if(ok){
                                    uint64_t delta = geneticDistance(thisGenome[parents[l]], thisGenome[m]);
                                    if(delta > maxDelta){
                                        maxDelta = delta;
                                    }
                                }
                            }
                        }
                        for(int l=0;l<substage.individuals;++l){
                            float bestDelta = FLT_MAX;
                            int bestMatchIndex = -1;
                            for(int m=0;m<populationSize;++m){
                                bool ok = true;
                                for(int o=0;o<substage.individuals;++o){
                                    if(m==parents[o]){
                                        ok = false;
                                        break;
                                    }
                                }
                                if(ok){
                                    float delta = substage.param.desiredGeneticDistance - geneticDistance(thisGenome[parents[l]], thisGenome[m])/float(maxDelta);
                                    if(delta < bestDelta){
                                        bestDelta = delta;
                                        bestMatchIndex = m;
                                        if(bestDelta==0){
                                            break;
                                        }
                                    }
                                }
                            }
                            twoPointsCrossover((uint8_t*)(thisGenome + parents[l]), (uint8_t*)(thisGenome + bestMatchIndex), sizeof(Genome_t), (uint8_t*) nextGenome + individualsGenerated + l, genesLoci, stemCellsTypes*fieldsNumber + fieldsNumber - 1);
                            invalidatedBodies.push_back(individualsGenerated + l);
                        }
                        break;
                    }
                    case SelectionSubstage::UNIFORM_CO:{
                        //We select half of the parents according to their fitness with a roulette wheel, 
                        //then they select their partners among the rest of the population according to genetic similarity, aiming for substage.param.desiredGeneticDistance
                        int parents[substage.individuals];
                        rouletteRanking(populationSize, currentFitness, parents, substage.individuals);
                        uint64_t maxDelta = 0;
                        for(int l=0;l<substage.individuals;++l){
                            for(int m=0;m<populationSize;++m){
                                bool ok = true;
                                for(int o=0;o<substage.individuals;++o){
                                    if(m==parents[o]){
                                        ok = false;
                                        break;
                                    }
                                }
                                if(ok){
                                    uint64_t delta = geneticDistance(thisGenome[parents[l]], thisGenome[m]);
                                    if(delta > maxDelta){
                                        maxDelta = delta;
                                    }
                                }
                            }
                        }
                        for(int l=0;l<substage.individuals;++l){
                            float bestDelta = FLT_MAX;
                            int bestMatchIndex = -1;
                            for(int m=0;m<populationSize;++m){
                                bool ok = true;
                                for(int o=0;o<substage.individuals;++o){
                                    if(m==parents[o]){
                                        ok = false;
                                        break;
                                    }
                                }
                                if(ok){
                                    float delta = substage.param.desiredGeneticDistance - geneticDistance(thisGenome[parents[l]], thisGenome[m])/float(maxDelta);
                                    if(delta < bestDelta){
                                        bestDelta = delta;
                                        bestMatchIndex = m;
                                        if(bestDelta==0){
                                            break;
                                        }
                                    }
                                }
                            }
                            uniformCrossover((uint8_t*)(thisGenome + parents[l]), (uint8_t*)(thisGenome + bestMatchIndex), sizeof(Genome_t), (uint8_t*) nextGenome + individualsGenerated + l, genesLoci, stemCellsTypes*fieldsNumber + fieldsNumber - 1);
                            invalidatedBodies.push_back(individualsGenerated + l);
                        }
                        break;
                    }
                    case SelectionSubstage::MUTATE:{
                        //Inside-out Fisher-Yattes Shuffle to get a random permutation of [0, populationSize-1]
                        int permutation[populationSize];
                        for(int l=0;l<populationSize;++l){
                            std::uniform_int_distribution<> dis(0, l);
                            int index = dis(gen);
                            if(index!=l){
                                permutation[l] = permutation[index];
                            }
                            permutation[index] = l;
                        }
                        for(int l=0;l<substage.individuals;++l){
                            mutateGenome(thisGenome + permutation[l], substage.param.mutationProbability);
                            nextGenome[individualsGenerated + l] = thisGenome[permutation[l]];
                            invalidatedBodies.push_back(individualsGenerated + l);
                        }
                        break;
                    }
                }
                if(!(substage.type&SelectionSubstage::TWO_POINTS_CO)){
                    for(int l=0;l<substage.individuals;++l){
                        nextGenome[individualsGenerated + l] = thisGenome[winners[l]];
                    }
                    for(int l=0;l<substage.individuals;++l){
                        uint8_t *dummy = nextGen[individualsGenerated+l];
                        nextGen[individualsGenerated+l] = thisGen[winners[l]];
                        thisGen[winners[l]] = dummy;
                    }
                    for(int l=0;l<substage.individuals;++l){
                        nextFitness[individualsGenerated + l] = currentFitness[winners[l]];
                    }
                }
                individualsGenerated+=substage.individuals;
            }
            uint8_t **dummy = nextGen;
            nextGen = thisGen;
            thisGen = dummy;
            float *fitnessDummy = nextFitness;
            nextFitness = currentFitness;
            currentFitness = fitnessDummy;
            Genome_t *genomeDummy = nextGenome;
            nextGenome = thisGenome;
            thisGenome = genomeDummy;
            std::sort(invalidatedBodies.begin(), invalidatedBodies.end());
            for(uint64_t k=0;k<invalidatedBodies.size();++k){
                std::cout<<"Developing genome "<<invalidatedBodies[k]<<std::endl;
                loadGenome(&handles, thisGenome + invalidatedBodies[k]);
                developBody(&handles, developmentStages);
                birthBody(thisGen[invalidatedBodies[k]]);
            }
            if(plan.stages[i].weights[0] + plan.stages[i].weights[1]*j==previousWeights){
                    for(int k : invalidatedBodies){
                        isolateBody(&dummyBody, thisGen[k]);
                        currentFitness[k] = fitnessFunction(&dummyBody, plan.targets, plan.stages[i].weights[0] + plan.stages[i].weights[1] * j);
                    }
            } else {
                    for(int k=0;k<populationSize;++k){
                        isolateBody(&dummyBody, thisGen[k]);
                        currentFitness[k] = fitnessFunction(&dummyBody, plan.targets, plan.stages[i].weights[0] + plan.stages[i].weights[1] * j);
                    }
            }
            invalidatedBodies.clear();
            previousWeights = plan.stages[i].weights[0] + plan.stages[i].weights[1]*j;
        }
    }
    delete[] dummyBody.cells;
    if(genomes!=thisGenome){
        std::memcpy(genomes, thisGenome, populationSize*sizeof(Genome_t));
    }
    delete[] nextGenome;
    for(int i=0;i<populationSize;++i){
        isolateBody(bodies + i, thisGen[i]);
    }
    for(int i=0;i<populationSize;++i){
        delete[] thisGen[i];
    }
    delete[] thisGen;
    for(int i=0;i<populationSize;++i){
        delete[] nextGen[i];
    }
    delete[] nextGen;
    if(fitness!=currentFitness){
        std::memcpy(fitness, currentFitness, populationSize*sizeof(float));
    }
    delete[] nextFitness;
    deleteHandles(&handles);
}
