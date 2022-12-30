#include <iostream>
#include <string>
#include <cassert>
#include <unordered_map>
#include <algorithm>
#include <memory>

// @author Харламенко Игорь Викторович
// @date 30.10.2022
// затраченное время на выполнение теста - около 5-ти часов 
// (включая реализацию двусвязного списка, тестирование его сериализации/десериализации,
// а также тестирование задач #1 и #2)

// 1. Напишите функцию, которая принимает на вход знаковое целое число и печатает его двоичное 
// представление, не используя библиотечных классов или функций.
void integerToBinString(int number)
{
    std::string result;

    unsigned bitOne = 1 << 31;
    while (bitOne) {
        result.push_back('0' + !!(number & bitOne));
        bitOne /= 2;
    }
       
    std::cout << result << std::endl;
}

// 2. Напишите функцию, удаляющую последовательно дублирующиеся символы в строке:
void removeDups(char* str)
{
    int r{}, l{};

    if (str[r] == '\0') return;

    while (str[r] != '\0') {
        if (str[r] != str[l])
            str[++l] = str[r];

        r++;
    }

    str[++l] = '\0';
}

// 3. Реализуйте функции сериализации и десериализации двусвязного списка в бинарном формате 
// в файл. Алгоритмическая сложность решения должна быть меньше квадратичной.
struct ListNode
{
    ListNode* pPrev;
    ListNode* pNext;
    ListNode* pRand;
    std::string data;

    ListNode() : pPrev(nullptr), pNext(nullptr), pRand(nullptr) {};
};

class List
{
public:

    void serialize(FILE* pFile) const {

        if (!pFile || mSize == 0) return;

        ListNode* pNode = mpHead->pNext;

        // Формируем проекцию указателей нод на индексы массива.
        // При формировании проекции проверяем два условия: 1 - итератор дошел до конца списка,
        //                                                  2 - итератор вошел в прямой цикл
        std::unordered_map <ListNode*, size_t> nodeMap;
        size_t nodeIndex{};
        auto it = nodeMap.find(pNode);

        // Резервируем индекс '0' для значения nullptr, поэтому, нумерация нод начинается с 1
        while (pNode != mpTail && it == nodeMap.end()) {
            nodeMap[pNode] = ++nodeIndex;
            pNode = pNode->pNext;
            it = nodeMap.find(pNode);
        }

        pNode = mpHead->pNext;
        size_t size = nodeMap.size();

        // Первым элементом в файл записываем количество нод в списке 
        if (1 != fwrite(reinterpret_cast <char*> (&size), sizeof(size_t), 1, pFile))
            return;

        // Формирование дескрипторов нод и запись их в файл
        size_t elementsWritten{};
        while (elementsWritten < nodeMap.size())
        {
            // Заполняем дескриптор очередной ноды.
            // Если указатель не найден в map, то индекс заносится в поле дескриптора как 0. При
            // десериализации, такой индекс будет восстановлен как nullptr.
            SerializationDescriptor node;
            node.dataSize      = pNode->data.size();
            node.nextNodeIndex = (nodeMap.find(pNode->pNext) != nodeMap.end()) ? nodeMap[pNode->pNext] : 0;
            node.prevNodeIndex = (nodeMap.find(pNode->pPrev) != nodeMap.end()) ? nodeMap[pNode->pPrev] : 0;
            node.randNodeIndex = (nodeMap.find(pNode->pRand) != nodeMap.end()) ? nodeMap[pNode->pRand] : 0;

            // Записываем дескриптор очередной ноды
            if (1 != fwrite(reinterpret_cast <char*> (&node), sizeof(node), 1, pFile))
                return;

            // Записываем данные очередной ноды
            if (1 != fwrite(pNode->data.c_str(), pNode->data.size(), 1, pFile))
                return;

            // Двигаемся к следующей ноде
            pNode = pNode->pNext;
            elementsWritten++;
        }
    }

    void deserialize(FILE* pFile) {
        if (!pFile) return;

        size_t nodesCount{};

        // Если не удалось прочитать количество нод из файла - прекращаем десериализацию
        if (1 != fread(reinterpret_cast <char*> (&nodesCount), sizeof(size_t), 1, pFile))
            return;

        // Стараемся безопасно прочитать ноды из файла, так, чтобы не испортить текущий список.
        // Для этого будем использовать map с парой <дескриптор ноды, умный указатель на экземпляр новой ноды> 
        // в качестве ключа, чтобы в случае неудачного чтения хотя бы одного элемента,
        // прекратить чтение и автоматичски освободить выделенную под новые ноды память.
        // Чтение ведется от 1, потому что индекс '0' зарезервирован для значения nullptr
        std::unordered_map <size_t, std::pair <SerializationDescriptor, std::shared_ptr <ListNode>>> nodeMap;

        for (size_t i = 1; i < nodesCount+1; i++)
        {
            nodeMap[i] = std::pair <SerializationDescriptor, std::shared_ptr <ListNode>>(SerializationDescriptor(),
                                                                                         std::make_shared <ListNode>());

            // Читаем дескриптор очередной ноды
            if (1 != fread(reinterpret_cast <char*> (&nodeMap[i].first), sizeof(SerializationDescriptor), 1, pFile))
                return;

            // Необходимо удостовериться, что запрашиваемый объем памяти для данных может быть выделен
            try{
                nodeMap[i].second->data = std::string(nodeMap[i].first.dataSize, '\0');
            }
            catch(std::bad_alloc&) {return;}
            
            // Читаем данные очередной ноды
            if (1 != fread(reinterpret_cast <char*> (&nodeMap[i].second->data[0]), nodeMap[i].first.dataSize, 1, pFile))
                return;
        }

        // После успешного чтения информации о нодах из файла, можем очистить текущий список
        clear();

        // Финальная часть десериализации
        for (size_t i = 1; i < nodesCount + 1; i++)
        {
            SerializationDescriptor& descriptor = nodeMap[i].first;
            std::shared_ptr <ListNode>& node    = nodeMap[i].second;

            // Восстанавливаем отношения между нодами
            node->pNext = (descriptor.nextNodeIndex == 0) ? nullptr : nodeMap[descriptor.nextNodeIndex].second.get();
            node->pPrev = (descriptor.nextNodeIndex == 0) ? nullptr : nodeMap[descriptor.prevNodeIndex].second.get();
            node->pRand = (descriptor.nextNodeIndex == 0) ? nullptr : nodeMap[descriptor.randNodeIndex].second.get();

            //! Формируем очередную ноду и помещаем в неё содержимое, считанное из файла
            ListNode* pNode = new ListNode;
            std::swap(*pNode, *node);

            // Помещаем очередную ноду в конец списка
            pNode->pNext = mpTail;
            pNode->pPrev = mpTail->pPrev;
            pNode->pPrev->pNext = pNode;
            mpTail->pPrev = pNode;
            mSize++;
        }
    }

    void clear()
    {
        ListNode* pNode = mpHead->pNext;

        while (pNode != mpTail)
        {
            pNode->pPrev->pNext = pNode->pNext;
            pNode->pNext->pPrev = pNode->pPrev;
            if (!pNode) delete pNode;
            pNode = mpHead->pNext;
            mSize--;
        }
    }

private:

    struct SerializationDescriptor
    {
        size_t randNodeIndex{};
        size_t prevNodeIndex{};
        size_t nextNodeIndex{};
        size_t dataSize;

        SerializationDescriptor() : randNodeIndex(0),
                                    prevNodeIndex(0),
                                    nextNodeIndex(0),
                                    dataSize(0)
        {};
    };

    ListNode* mpHead;
    ListNode* mpTail;
    size_t mSize;
};


int main()
{

    return EXIT_SUCCESS;
}