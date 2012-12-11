#ifndef PAXOS_HH
#define PAXOS_HH

#include <QObject>
#include <QMap>
#include <QVariant>
#include <QString>
#include <QVariantMap>
#include <QTimer>
#include <QUuid>
#include <QSet>
#include <assert.h>

#define PAXOS_REQUEST_TIMEOUT 3000

class ProposalNumber : public QObject{

  Q_OBJECT

public:
  ProposalNumber();
  
  ProposalNumber(quint64 number, QString name);
  
  ProposalNumber(const ProposalNumber &other);
  
  ~ProposalNumber();
  
  bool
  operator> (const ProposalNumber & second) const;
  
  bool
  operator>= (const ProposalNumber &second) const;

  bool
  operator== (const ProposalNumber &second) const;

  bool
  operator< (const ProposalNumber &second) const;

  bool
  operator<= (const ProposalNumber &second) const;
  
  ProposalNumber
  operator= (const ProposalNumber &second);
  
  static ProposalNumber 
  incr(const ProposalNumber &p);  
  
  quint64 number;
  QString name;
  
};

Q_DECLARE_METATYPE(ProposalNumber)

enum PaxosCodes {
  
  // From the proposer.
  PHASE1 = 1,
  PHASE2 = 3,
  COMMIT = 5,
  IDLE = 7,
  PROCESSING = 9,
  

  // From the acceptor.
  REJECT = 0,
  PROMISEVALUE = 2,
  PROMISENOVALUE = 4,
  ACCEPT = 6,
};

class Proposer : public QObject {

  Q_OBJECT

public:

  Proposer(quint32 given_majority);  

signals:

    void
    catchupInstance(quint32 round, QVariantMap value);					   
    void
    proposalTimeout();
    void
    broadcastMessage(const QVariantMap& msg);

public slots:
  
  void 
  phase1 (quint32 round, QVariantMap value);
  void
  processFailed(QVariantMap response);
  void
  processPromise(const QVariantMap& response);
  void
  processAccept(QVariantMap response);

private:

  void
  phase2();
  QPair<ProposalNumber, QVariantMap> 
  checkAccepted();
  void
  processTimeout();
  void
  broadcastCommit(ProposalNumber p);
  
  PaxosCodes state;
  quint32 majority;
  
  QTimer roundTimer;
  ProposalNumber curProposal;
  
  QVariantMap curValue;
  QVariantMap acceptValue;
  
  quint32 curRound;

  QSet<QString> uniquePhase1Replies;
  QList<QVariantMap> phase1Replies;
  QSet<QString> uniquePhase2Replies;
};

class Acceptor : public QObject {
  
  Q_OBJECT

public:  
  Acceptor();

signals:
    void
    singleReceiver(const QVariantMap &msg, const QString origin);

public slots:
  void
  tryPromise(const QVariantMap& msg);
  void
  tryAccept(const QVariantMap& msg);
 
private:

  QMap<quint32, ProposalNumber> maxPromise;
  QMap<quint32, QVariantMap> acceptValues;
  QMap<quint32, ProposalNumber> acceptProposals;  
};

class Paxos : public QObject {
  
  Q_OBJECT


public:

  Paxos(QList<QString> participants);
  
public slots:
  
  void
  broadcastMsg(const QVariantMap &msg);
  void
  newMessage(const QVariantMap & msg);
  void
  sendSingle(const QVariantMap& reply, const  QString& dest);
  void
  clientRequest(const QString& value);
  void
  proposerTimeoutFailure();
  void
  laggingRoundNumber(quint32 round, const QVariantMap& value);
  
signals:

  void
  rejectMessage(const QVariantMap& msg);
  void
  promiseMessage(const QVariantMap& msg);
  void
  acceptMessage(const QVariantMap& msg);
  void
  phase1Message(const QVariantMap& msg);
  void
  phase2Message(const QVariantMap& msg);
  void
  commitMessage(const QVariantMap& msg);
  void
  newValue(quint32 round, const QString &value);
  void
  sendP2P(const QVariantMap& reply, const QString& destination);

private:
  
  QString
  newId();
  void
  processPhase1(const QVariantMap& msg);
  
  void
  commit(QVariantMap msg);

  QMap<quint32, QVariantMap> commits;
  
  quint32 maxSafeRound;
  
  QList<QVariantMap> pendingRequests;
  
  QList<QString> participants;

  QString me;
  
  Proposer *proposer;
  Acceptor *acceptor;
};

#endif