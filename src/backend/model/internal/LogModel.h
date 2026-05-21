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


#pragma once

#include <QObject>
#include <QStringList>

namespace model {
class LogModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(QStringList messages READ messages NOTIFY messagesChanged)
    Q_PROPERTY(int count READ count NOTIFY messagesChanged)

public:
    explicit LogModel(QObject* parent = nullptr);

    QStringList messages() const;
    int count() const;

    void addMessage(const QString& msg);

public slots:
    void clear();

signals:
    void messagesChanged();

private:
    QStringList m_messages;
};
} // namespace model
