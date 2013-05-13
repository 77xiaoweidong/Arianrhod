#include "edao.h"

BOOL EDAO::CheckItemEquipped(ULONG ItemId, PULONG EquippedIndex)
{
    switch (ItemId)
    {
        case 0xB7:  // ��Ŀ
        case 0xB8:  // ����
        case 0xBB:  // ֪̽
            if (EquippedIndex != NULL)
                *EquippedIndex = 0;
        
            return TRUE;
    }
    
    return (this->*StubCheckItemEquipped)(ItemId, EquippedIndex);
}
