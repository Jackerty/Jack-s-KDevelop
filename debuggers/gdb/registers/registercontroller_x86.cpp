/*
 * Class to fetch/change/send registers to the debugger for x86, x86_64 architectures.
 * Copyright 2013  Vlas Puhov <vlas.puhov@mail.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "registercontroller_x86.h"

#include "../debugsession.h"
#include "../gdbcommand.h"
#include "../mi/gdbmi.h"

#include <qmath.h>
#include <QRegExp>

#include <KDebug>
#include <KLocalizedString>

namespace GDBDebugger
{

QVector<QStringList> RegisterControllerGeneral_x86::m_registerNames;
FlagRegister RegisterControllerGeneral_x86::m_eflags;

void RegisterControllerGeneral_x86::updateValuesForRegisters(RegistersGroup* registers) const
{
    kDebug() << "Updating values for registers: " << registers->groupName;
    if (registers->groupName == enumToString(Flags)) {
        updateFlagValues(registers);
    } else {
        IRegisterController::updateValuesForRegisters(registers);
    }
}

RegistersGroup RegisterControllerGeneral_x86::registersFromGroup(const QString& group, RegistersFormat format) const
{
    RegistersGroup registers;

    registers.groupName = group;
    registers.editable = (group == enumToString(XMM)) ? false : true;
    registers.flag = (group == enumToString(Flags)) ? true : false;
    foreach (const QString & name, registerNamesForGroup(group)) {
        registers.registers.append(Register(name, QString()));
    }

    updateValuesForRegisters(&registers);
    convertValuesForGroup(&registers, format);

    return registers;
}

QStringList RegisterControllerGeneral_x86::namesOfRegisterGroups() const
{
    static const QStringList registerGroups = QStringList() << enumToString(General) << enumToString(Flags) << enumToString(FPU) << enumToString(XMM) << enumToString(Segment);

    return registerGroups;
}

void RegisterControllerGeneral_x86::updateFlagValues(RegistersGroup* flagsGroup) const
{
    int flagsValue = registerValue(m_eflags.registerName).toInt(0, 16);

    for (int idx = 0; idx < m_eflags.flags.count(); idx++) {
        flagsGroup->registers[idx].value = ((flagsValue >> m_eflags.bits[idx].toInt()) & 1) ? "1" : "0";
    }
}

void RegisterControllerGeneral_x86::setRegisterValueForGroup(const QString& group, const Register& reg)
{
    if (group == enumToString(General)) {
        setGeneralRegister(reg, group);
    } else if (group == enumToString(Flags)) {
        setFlagRegister(reg, m_eflags);
    } else if (group == enumToString(FPU)) {
        setFPURegister(reg);
    } else if (group == enumToString(XMM)) {
        setXMMRegister(reg);
    } else if (group == enumToString(Segment)) {
        setSegmentRegister(reg);
    } else {
        Q_ASSERT(0);
    }
}

void RegisterControllerGeneral_x86::setFPURegister(const Register& reg)
{
    setGeneralRegister(reg, enumToString(FPU));
}

void RegisterControllerGeneral_x86::setXMMRegister(const Register& /*reg */)
{
    //FIXME:
    //setGeneralRegister ( reg, enumToString ( XMM ) );
}

void RegisterControllerGeneral_x86::setSegmentRegister(const Register& reg)
{
    setGeneralRegister(reg, enumToString(Segment));
}

void RegisterControllerGeneral_x86::updateRegisters(const QString& group)
{
    if (!m_debugSession || m_debugSession->stateIsOn(s_dbgNotStarted | s_shuttingDown)) {
        return;
    }

    if (!m_registerNamesInitialized) {
        if (initializeRegisters()) {
            m_registerNamesInitialized = true;
        }
    }

    if (group != enumToString(FPU)) {
        if (group.isEmpty()) {
            QStringList groups = namesOfRegisterGroups();
            groups.removeOne(enumToString(FPU));
            foreach (const QString & g, groups) {
                IRegisterController::updateRegisters(g);
            }
        } else {
            IRegisterController::updateRegisters(group);
        }
    }

    if (group == enumToString(FPU) || group.isEmpty()) {
        if (m_debugSession && !m_debugSession->stateIsOn(s_dbgNotStarted | s_shuttingDown)) {

            QString command = "info all-registers ";
            foreach (const QString & name, registerNamesForGroup(enumToString(FPU))) {
                command += "$" + name + ' ';
            }
            //TODO: use mi interface instead.
            m_debugSession->addCommand(
                new CliCommand(GDBMI::NonMI, command, this, &RegisterControllerGeneral_x86::handleFPURegisters));
        }
    }
}

QString RegisterControllerGeneral_x86::enumToString(X86RegisterGroups group) const
{
    static const QString groups[LAST_REGISTER] = {i18n("General"), i18n("Flags"), i18n("FPU"), i18n("XMM"), i18n("Segment")};

    return groups[group];
}

void RegisterControllerGeneral_x86::convertValuesForGroup(RegistersGroup* registersGroup, RegistersFormat format) const
{
    if (format != Raw && format != Natural) {
        if (registersGroup->groupName == enumToString(General) || registersGroup->groupName == enumToString(Segment)) {
            IRegisterController::convertValuesForGroup(registersGroup, format);
        }
    }
}

void RegisterControllerGeneral_x86::handleFPURegisters(const QStringList& record)
{
    //st0       0
    //st1       -1
    //st2       123456789
    //st3       -12345.6789
    //st4       12.34
    //st5       1.1e+2
    //st6       -23.45e-10
    const QRegExp rx("^(st[0-8])\\s+((?:-?\\d+\\.?\\d+(?:e(\\+|-)\\d+)?)|(?:-?\\d+))$");
    QVector<Register> registers;
    foreach (const QString & s, record) {
        if (rx.exactMatch(s)) {
            registers.push_back(Register(rx.cap(1), rx.cap(2)));
        }
    }

    if (registers.size() != 8) {
        kDebug() << "can't parse FPU. Wrong format" << record << "registers: ";
        foreach (const Register & r, registers) {
            kDebug() << r.name << ' ' << r.value;
        }
    } else {
        foreach (const Register & r, registers) {
            if (m_registers.contains(r.name)) {
                m_registers[r.name] = r.value;
            }
        }
        emit registersInGroupChanged(enumToString(FPU));
    }
}

RegisterController_x86::RegisterController_x86(DebugSession* debugSession, QObject* parent)
    : RegisterControllerGeneral_x86(debugSession, parent)
{
    initRegisterNames();
}

RegisterController_x86_64::RegisterController_x86_64(DebugSession* debugSession, QObject* parent)
    : RegisterControllerGeneral_x86(debugSession, parent)
{
    initRegisterNames();
}

RegisterControllerGeneral_x86::RegisterControllerGeneral_x86(DebugSession* debugSession, QObject* parent)
    : IRegisterController(debugSession, parent), m_registerNamesInitialized(false)
{
    if (m_registerNames.isEmpty()) {
        for (int i = 0; i < static_cast<int>(LAST_REGISTER); i++) {
            m_registerNames.append(QStringList());
        }
        initRegisterNames();
    }
}

void RegisterControllerGeneral_x86::initRegisterNames()
{
    for (int i = 0; i < 8; i++) {
        m_registerNames[FPU] << ("st" + QString::number(i));
    }

    m_registerNames[Flags] << "C" << "P" << "A" << "Z" << "S" << "T" << "D" << "O";

    m_registerNames[Segment] << "cs" << "ss" << "ds" << "es" << "fs" << "gs";

    m_eflags.flags = m_registerNames[Flags];
    m_eflags.bits << "0" << "2" << "4" << "6" << "7" << "8" << "10" << "11";
    m_eflags.registerName = "eflags";
    m_eflags.groupName = enumToString(Flags);
}

void RegisterController_x86::initRegisterNames()
{
    m_registerNames[General] = QStringList() << "eax" << "ebx" << "ecx" << "edx" << "esi" << "edi" << "ebp" << "esp" << "eip";

    m_registerNames[XMM].clear();
    for (int i = 0; i < 8; i++) {
        m_registerNames[XMM] << ("xmm" + QString::number(i));
    }
}

void RegisterController_x86_64::initRegisterNames()
{
    m_registerNames[General] = QStringList() << "rax" << "rbx" << "rcx" << "rdx" << "rsi" << "rdi" << "rbp" << "rsp" << "r8" << "r9" << "r10" << "r11" << "r12" << "r13" << "r14" << "r15" << "rip";

    m_registerNames[XMM].clear();
    for (int i = 0; i < 16; i++) {
        m_registerNames[XMM] << ("xmm" + QString::number(i));
    }
}

QStringList RegisterControllerGeneral_x86::registerNamesForGroup(const QString& group) const
{
    for (int i = 0; i < static_cast<int>(LAST_REGISTER); i++) {
        if (group == enumToString(static_cast<X86RegisterGroups>(i))) {
            return m_registerNames[i];
        }
    }

    return QStringList();
}

}
