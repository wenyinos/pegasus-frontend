// Pegasus Frontend
// Copyright (C) 2017-2024  Mátyás Mustoha
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.


#include "LogModel.h"

namespace model {

LogModel::LogModel(QObject* parent)
    : QObject(parent)
{}

QStringList LogModel::messages() const
{
    return m_messages;
}

int LogModel::count() const
{
    return m_messages.size();
}

void LogModel::addMessage(const QString& msg)
{
    m_messages.append(msg);
    emit messagesChanged();
}

void LogModel::clear()
{
    if (m_messages.isEmpty())
        return;

    m_messages.clear();
    emit messagesChanged();
}

} // namespace model
