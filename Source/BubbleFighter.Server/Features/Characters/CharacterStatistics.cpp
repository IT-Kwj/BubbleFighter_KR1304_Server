#include "CharacterStatistics.h"

namespace BubbleFighter::Server::Features::Characters
{
    bool CharacterStatistics::IsValid() const
    {
        // Local storage bounds keep values nonnegative in signed client logic.
        // This is not evidence of the game's maximum level or wallet balance.
        return Level >= 1 && Level <= MaxValue && Experience <= MaxValue &&
            Lucci <= MaxValue && Cash <= MaxValue;
    }
}
