## evolution-gpu

# Description
This repository consists of an individual templated header[^1].
The header defines a handful of templated struct types, and an `evolve` function which uses those templated types and a function parameter that uses those templated types.
For the end-user this resolves into having to define a fitness function that takes a 'Body', as defined in ['evo-devo-gpu'][evo-devo-gpu], 
and two user-defined structs that describe the ideal values of some user-defined quantity the body should have, together with the weights each of those quantities should have in calculating the fitness of said body.  
Once the fitness function has been defined, it's time to describe what sort of genetic algorithm the user wants to use: this is done by specializing the templated struct types
defined in the header with the Target and Weights struct types the user has defined in the previous step, and then instantiating a single 'SelectionPlan', 
which will in turn have a set amount of Stages; each stage consists of Substages, which correspond to the possible ways to generate new individuals defined in ['genetic-algorithm--'][genetic-algorithm--].
Make sure that the sum of the individuals processed by the substages in a stage is equal to the total population size.
At this point the only thing left before calling evolve is to allocate and call 'generateGenome' on an array of 'Genome_t' variables.
Call 'evolve' with the parameters you've created and, after a while, you'll get your results.

# Dependencies

This repository depends on ['evo-devo-gpu'][evo-devo-gpu] and ['genetic-algorithm--'][genetic-algorithm--], and as such inherits the first one's requirement of OpenGL 4.5


[evo-devo-gpu]:https://github.com/tesseract241/evo-devo-gpu

[genetic-algorithm--]:https://github.com/tesseract241/genetic-algorithm--

[^1]: This is not a choice but is dependent on C++'s compilation model, which uses separate translation units. As such, a template definition needs to be either included in the source using it, or the header itself needs to explicitly implement the specialization of the templates. For our intended use-case, and for libraries in general, only the first option is really viable.
