#ifndef preprocessors_h
#define preprocessors_h

#define CONCAT_(prefix, suffix) prefix ## suffix
#define CONCAT(prefix, suffix) CONCAT_(prefix, suffix)

#define ifGui(condition, popStyleCode) \
	bool CONCAT(ifGui, __LINE__) = condition;\
	{popStyleCode;}\
	if (CONCAT(ifGui, __LINE__))

#endif /* preprocessors_h */
