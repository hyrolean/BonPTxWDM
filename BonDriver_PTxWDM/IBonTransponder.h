// IBonTransponder.h: IBonTransponder �N���X�̃C���^�[�t�F�C�X
//
//////////////////////////////////////////////////////////////////////

#if !defined(_IBONTRANSPONDER_H_)
#define _IBONTRANSPONDER_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// �}�g�����X�|���_�C���^�[�t�F�C�X (TSID�ɂ��`���[�j���O�@�\���܂Ƃ߂�����)
class IBonTransponder
{
public:

	// �������ň����Ă���ID�Ƃ́A�g�����X�|�[�g�X�g���[��ID(TSID)�̂��Ƃ�\��

	// �g�����X�|���_����񋓂���
	//  �� �X�y�[�X�͈̔͂́AIBonDriver2::EnumTuningSpace �Ŏ擾�������̂ł���ɓ�������
	//  �� dwTransponder�́A0�`�w�肵��NULL��Ԃ��܂ł͈̔͂ŗL��
	virtual LPCTSTR TransponderEnumerate(const DWORD dwSpace, const DWORD dwTransponder) = 0;

	// ���݂̃X�y�[�X�ƌ��s�g�����X�|���_��I������
	//  �� �X�y�[�X�͈̔͂́AIBonDriver2::EnumTuningSpace �Ŏ擾�������̂ł���ɓ�������
	//  �� dwTransponder�́A0�`�w�肵��FALSE��Ԃ��܂ł͈̔͂ŗL��
	//  �� �g�����X�|���_�f�R�[�_���̂��L���łȂ��i�n�f�W�Ȃǁj�ꍇ�́A���FALSE��Ԃ�
	virtual const BOOL TransponderSelect(const DWORD dwSpace, const DWORD dwTransponder) = 0;

	// ���݂̃g�����X�|���_�f�R�[�_����ݒ�ł���X�g���[��ID�̈ꗗ���擾����
	//  lpIDList �� NULL�w��:
	//    *lpdwNumID�Ɋ��蓖�ĂɕK�v�ȃX�g���[����(default:8)���Ԃ�
	//  ����ȊO:
	//    lpIDList�z���ID���X�g��,*lpdwNumID�Ɏ擾���������Z�b�g���ĕԂ�
	//  �� ���ݕ�������Ă��Ȃ��X�g���[���ɂ́A0xFFFFFFFF �����邱�ƂƂ���
	//  �� �g�����X�|���_�f�R�[�_���̂��L���łȂ��i�n�f�W�Ȃǁj�ꍇ�́A���FALSE��Ԃ�
	virtual const BOOL TransponderGetIDList(LPDWORD lpIDList, LPDWORD lpdwNumID) = 0;

	// ���s�g�����X�|���_�̃f�R�[�h�ΏۃX�g���[��ID��ݒ肷��
	//  �� �g�����X�|���_�f�R�[�_���̂��L���łȂ��i�n�f�W�Ȃǁj�ꍇ�́A���FALSE��Ԃ�
	virtual const BOOL TransponderSetCurID(const DWORD dwID) = 0;

	// �f�R�[�h�ΏۃX�g���[��ID�����s�g�����X�|���_�̃f�R�[�_����擾����
	//  �� �������A�X�g���[���������ȏꍇ�ɂ́A0xFFFFFFFF �� *lpdwID �ɑ������
	//  �� �g�����X�|���_�f�R�[�_���̂��L���łȂ��i�n�f�W�Ȃǁj�ꍇ�́A���FALSE��Ԃ�
	virtual const BOOL TransponderGetCurID(LPDWORD lpdwID) = 0;

	// �� TransponderSelect ���Ă΂�ă`�����l�����ύX���ꂽ�ꍇ�A
	//    IBonDriver2::GetCurChannel �́A�`�����l���ԍ��ł͂Ȃ��A
	//    Transponder�̃C���f�b�N�X�� 0x80000000 ���}�X�N�������̂�Ԃ�
	//
	//    �܂�AIBonDriver2::GetCurChannel�Ō��݂̑I�Ǐ�Ԃ��c���\
	//
	//    �EIBonDriver2::GetCurChannel�̕ԋp�l��31�r�b�g�ڂ������Ă��Ȃ�
	//       ��IBonDriver::SetChannel/IBonDriver2::SetChannel�Ń`�����l���ؑւ���Ă���
	//
	//    �EIBonDriver2::GetCurChannel�̕ԋp�l��31�r�b�g�ڂ������Ă���
	//       ��TransponderSelect�Ń`�����l���ؑւ���Ă���
	//       �� 0�`30�r�b�g�����̓`�����l���ԍ��ł͂Ȃ��ATransponder�̃C���f�b�N�X���Ӗ�����

};

// �g�����X�|���_�̃`�����l���}�X�N
// (�g�����X�|���_�I�����ɂ��̒l�����݂̃`�����l���Ƀ}�X�N�����)
#define TRANSPONDER_CHMASK 0x80000000

#include "IBonDriver2.h"
// �}(�h���C�o2+�g�����X�|���_)�����C���^�[�t�F�C�X
class IBonDriver2Transponder : public IBonDriver2, public IBonTransponder {};

#include "IBonDriver3.h"
// �}(�h���C�o3+�g�����X�|���_)�����C���^�[�t�F�C�X
class IBonDriver3Transponder : public IBonDriver3, public IBonTransponder {};

#endif // !defined(_IBONTRANSPONDER_H_)
