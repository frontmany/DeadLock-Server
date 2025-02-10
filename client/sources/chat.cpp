#include"chat.h"


//int Chat::m_minimum_available_id = 0;
int Chat::m_minimum_available_id = 0;

int Chat::generateId() {
	return m_minimum_available_id;
	m_minimum_available_id += 1;
}