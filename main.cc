
#include <unistd.h>
#include <ctime>
#include <cstdlib>

#include <QApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QTextStream>
#include <QtGlobal>
#include <QTime>
#include <QDateTime>
#include <QUuid>
#include <QListWidget>
#include <QtCrypto>
#include <QLabel>

#include "main.hh"
#include "router.hh"
#include "helper.hh"
#include "dispatcher.hh"

PaxosDialog::PaxosDialog(Router *r, const QList<QString> &participants)
{
  paxos = new Paxos(participants);
  valueDisplay = new QTextEdit(this);
  valueDisplay->setReadOnly(true);
  valueAdder = new QLineEdit(this);

  QVBoxLayout *layout = new QVBoxLayout();
  QLabel *learnedValues = new QLabel("Learned values:");
  QLabel *commitValue = new QLabel("Commit a value:");
  
  layout->addWidget(learnedValues);
  layout->addWidget(valueDisplay);
  layout->addWidget(commitValue);
  layout->addWidget(valueAdder);
  
  setLayout(layout);
  
  valueAdder->setFocus();

  // Connect our client to paxos.
  connect(this, SIGNAL(newRequest(const QString&)),
	  paxos, SLOT(clientRequest(const QString&)));

  connect(paxos, SIGNAL(newValue(quint32, const QString&)),
	  this, SLOT(newValue(quint32, const QString&)));
  

  // Connect the router to paxos.
  connect(paxos, SIGNAL(sendP2P(const QMap<QString, QVariant>&, const QString&)),
	  r, SLOT(sendMap(const QMap<QString, QVariant>&, const QString&)));
  connect(r, SIGNAL(toPaxos(const QMap<QString, QVariant>&)),
	  paxos, SLOT(newMessage(const QMap<QString, QVariant>&)));

  connect(valueAdder, SIGNAL(returnPressed()),
	  this, SLOT(gotReturnPressed()));
}

void
PaxosDialog::gotReturnPressed()
{
  emit newRequest (valueAdder->text());
  valueAdder->clear();
}

void
PaxosDialog::newValue(quint32 round,const  QString&  value)
{
  QString toDisplay = QString::number(round, 10);
  toDisplay += " :> ";
  toDisplay += value;
  valueDisplay->append(toDisplay);
}

FileDialog::FileDialog(FileRequests *fr)
{
  m_fr = fr;
  
  QLabel *entryLabel = new QLabel("Search here:");
  
  QVBoxLayout *layout = new QVBoxLayout();
  QHBoxLayout *innerLayout = new QHBoxLayout();

  text = new QLineEdit(this);
  innerLayout->addWidget(entryLabel);
  innerLayout->addWidget(text);
  
  fileButton = new QPushButton("Share File ...", this);
  fileButton->setDown(false);
  fileButton->setChecked(false);
  
  layout->addLayout(innerLayout);
  layout->addWidget(fileButton);
  setLayout(layout);
  
  connect(text, SIGNAL(returnPressed()),
	  this, SLOT(newSearch()));

  connect(this, SIGNAL(newRequest(const QString&)),
	  fr, SLOT(newSearch(const QString&)));

  connect(fr, SIGNAL(newResponse(const QString &, const QMap<QString, QVariant>&)),
	  this, SLOT(newSearchResult(const QString &, const QMap<QString, QVariant> &)));
  
  connect(this, SIGNAL(destroyRequest(const QString &)),
	  fr, SLOT(destroyRequest(const QString &)));

  connect(fileButton, SIGNAL(pressed()),
	  this, SLOT(fileButtonClicked()));		       
}


void
FileDialog::fileButtonClicked()
{
  //qDebug() << "file button clicked\n";
  
  // First disable the button from being
  // clicked until we're done selecting files.
  fileButton->setDown(true);
  
  // Setup a new file menu.
  fileMenu = new QFileDialog(this);
  fileMenu->setFileMode(QFileDialog::ExistingFiles);
  
  // Connect the signal for file selection to 
  // the corresponding slot.
  connect(fileMenu, SIGNAL(filesSelected(const QStringList &)),
	  this, SLOT(filesSelected(const QStringList &)));
  
  fileMenu->show();  
}

void
FileDialog::filesSelected(const QStringList & files)
{   
  
  int len = files.count();
  for(int i = 0; i < len; ++i)
    qDebug() << files[i] << '\n';

  //delete(fileMenu);
  fileButton->setDown(false);  
  emit indexFiles(files);
}


void
FileDialog::newSearch()
{
  QString query = text->text();
  text->clear();
  qDebug() << "New request!";
  if (!activeRequests.contains(query)){

    DownloadBox * newSearch = new DownloadBox(query);
    
    activeRequests[query] = newSearch;    
    
    connect(newSearch, SIGNAL(download(const QString&, const QString&, const QByteArray&)),
	    m_fr, SLOT(newDownload(const QString&, const QString&, const QByteArray&)));
    
    connect(newSearch, SIGNAL(close(const QString &)),
	    this, SLOT(closeBox(const QString& )));
    
    newSearch->show();
    emit newRequest(query);
  }  
}


void
FileDialog::newSearchResult(const QString &query, const QMap<QString, QVariant> &response)
{
  if (activeRequests.contains(query)){
    
    qDebug() << "FileDialog: got new search result for " << query;
    DownloadBox *search = activeRequests[query];
    search->newResult(response);
  }
}

void
FileDialog::closeBox(const QString &query)
{
  if (activeRequests.contains(query)){
    
    qDebug() << "Killed window";
    emit destroyRequest(query);
    DownloadBox *searchBox = activeRequests[query];
    activeRequests.remove(query);
    delete(searchBox);

  }
}

DownloadBox::DownloadBox(const QString& search)
{
  m_search = search;
  QString titleString = "Searching for... ";
  titleString.append(search);
  setWindowTitle(titleString);
  
  results = new QListWidget();
  layout = new QVBoxLayout();
  layout->addWidget(results);
  
  setLayout(layout);  

  connect(results, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
	  this, SLOT(gotDoubleClick(QListWidgetItem*)));
}

DownloadBox::~DownloadBox()
{
  results->~QListWidget();
  layout->~QVBoxLayout();
}

void
DownloadBox::newResult(const QMap<QString, QVariant> &msg)
{
  qDebug() << "DownloadBox: got new search result" << msg["Name"].toString();
  QListWidgetItem *item = new QListWidgetItem(msg["Name"].toString());
  item->setData(1, msg);
  results->addItem(item);
}

void
DownloadBox::gotDoubleClick(QListWidgetItem *item)
{
  QMap<QString, QVariant> data = (item->data(1)).toMap();

  qDebug() << "Start download!!!";
  emit download(data["Name"].toString(),
		data["Origin"].toString(),
		data["ID"].toByteArray());
}

void
DownloadBox::closeEvent(QCloseEvent *e)
{
  emit close(m_search);  
}



// BEGIN: PrivateChatDialog

PrivateChatDialog::PrivateChatDialog(const QString& destination)
{

  m_destination = destination;
  setWindowTitle(destination);


  textview = new QTextEdit(this);
  textview->setReadOnly(true);

  textentry = new QLineEdit(this);
  textentry->setFocus();

  layout = new QVBoxLayout();
  layout->addWidget(textview);
  layout->addWidget(textentry);

  connect(textentry, SIGNAL(returnPressed()),
	  this, SLOT(internalMessageReceived()));
  
  setLayout(layout);
}

PrivateChatDialog::~PrivateChatDialog()
{
  textview->~QTextEdit();
  textentry->~QLineEdit();
  layout->~QVBoxLayout();  
}

void
PrivateChatDialog::internalMessageReceived()
{
  QString msg = textentry->text();
  textview->append(msg);
  emit sendMessage(msg, m_destination);
  textentry->clear();
}


void
PrivateChatDialog::externalMessageReceived(const QString& msg)
{
  textview->append(msg);
}


void
PrivateChatDialog::closeEvent(QCloseEvent *e)
{
  
  emit closed(m_destination);
}

// END: PrivateChatDialog


// BEGIN: TextEntryWidget

TextEntryWidget::TextEntryWidget(QWidget * parent) : QTextEdit(parent)  
{
  
}

void TextEntryWidget::keyPressEvent(QKeyEvent *e)
{
  if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return){
    emit returnPressed();
  }
  else{
    QTextEdit::keyPressEvent(e);
  }
}

// END: TextEntryWidget


// Begin: ChatDialog

ChatDialog::ChatDialog(Router *r)
{
  router = r;
	setWindowTitle("Peerster");

	// Read-only text box where we display messages from everyone.
	// This widget expands both horizontally and vertically.
	textview = new QTextEdit(this);
	textview->setReadOnly(true);

	// Small text-entry box the user can enter messages.
	// This widget normally expands only horizontally,
	// leaving extra vertical space for the textview widget.
	//
	// You might change this into a read/write QTextEdit,
	// so that the user can easily enter multi-line messages.
	textline = new TextEntryWidget(this);


	
	peerAdder = new QLineEdit(this);


	// Lay out the widgets to appear in the main window.
	// For Qt widget and layout concepts see:
	// http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layouts.html
	QHBoxLayout *layout = new QHBoxLayout();
	
	QVBoxLayout *innerLayout = new QVBoxLayout();
	QVBoxLayout *innerLayout2 = new QVBoxLayout();
	
	QLabel *addPeer = new QLabel("Add a neighbor:");
	QLabel *typeHere = new QLabel("Type here for public chat:");
	innerLayout->addWidget(addPeer);
	innerLayout->addWidget(peerAdder);
	innerLayout->addWidget(textview);
	innerLayout->addWidget(typeHere);
	innerLayout->addWidget(textline);

	
	QLabel *originLabel = new QLabel("Double-click someone to private chat:");
	origins = new QListWidget();
	innerLayout2->addWidget(originLabel);
	innerLayout2->addWidget(origins);
	
	
	layout->addLayout(innerLayout);
	layout->addLayout(innerLayout2);
	
	setLayout(layout);
	

	textline->setFocus();
	// Register a callback on the textline's returnPressed signal
	// so that we can send the message entered by the user.
	connect(router, SIGNAL(newOrigin(const QString&)),
		this, SLOT(addOrigin(const QString&)));
	
	connect(textline, SIGNAL(returnPressed()),
		this, SLOT(gotReturnPressed()));


	connect(peerAdder, SIGNAL(returnPressed()),
		this, SLOT(gotAddPeer()));
	
	connect(origins, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
		this, SLOT(openEmptyPrivateChat(QListWidgetItem*)));

			 
	
}


void 
ChatDialog::addOrigin(const QString& origin)
{
  origins->addItem(origin);
}

void ChatDialog::gotAddPeer()
{
  QString temp = peerAdder->text();
  peerAdder->clear();
  ////qDebug() << "ChatDialog::gotAddPeer -- received request to add " << temp;
  emit this->addPeer(temp); 
}

void ChatDialog::gotReturnPressed()
{
	// Initially, just echo the string locally.
	// Insert some networking code here...
  ////qDebug() << "FIX: send message to other peers: " << textline->toPlainText();
  //textview->append(textline->toPlainText());

  emit this->sendMessage (textline->toPlainText());

	// Clear the textline to get ready for the next input message.
	textline->clear();

	
}

void ChatDialog::gotNewMessage(const QString& s)
{
  ////qDebug() << "ChatDialog::gotNewMessage -- ok, at least I get called" << '\n';
  textview->append(s);
}

void ChatDialog::destroyPrivateWindow(const QString & from)
{
  if(privateChats.contains(from)){
    
    PrivateChatDialog *privChat = privateChats[from];
    privateChats.remove(from);
    
    delete(privChat);
    //qDebug() << "Removed host from hash table!";
  }
}
void ChatDialog::newPrivateMessage(const QString& message, const QString& from)
{
  PrivateChatDialog *privChat;
  if (!privateChats.contains(from)){
    
    privChat = new PrivateChatDialog(from);
    privateChats[from] = privChat;
    QObject::connect(privChat, SIGNAL(sendMessage(const QString&, const QString&)),
		     router, SLOT(sendMessage(const QString&, const QString&)));

    connect(privChat, SIGNAL(closed(const QString&)),
	    this, SLOT(destroyPrivateWindow(const QString&)));
    privChat->show();
  }
  
  else{
    
    privChat = privateChats[from];
  }

  privChat->externalMessageReceived(message);  
}


void
ChatDialog::openEmptyPrivateChat(QListWidgetItem* item)
{
  PrivateChatDialog *privChat;
  QString destination = item->text();
  if (!privateChats.contains(destination)){
    
    privChat= new PrivateChatDialog(destination);
    privateChats[destination] = privChat;
    connect(privChat, SIGNAL(sendMessage(const QString&, const QString&)),
	    router, SLOT(sendMessage(const QString&, const QString&)));
    
    connect(privChat, SIGNAL(closed(const QString&)),
	    this, SLOT(destroyPrivateWindow(const QString&)));

    
    //qDebug() << "Opened empty private chat!";
    privChat->show();
  }
  
}


// End: ChatDialog

//Begin: NetSocket

NetSocket::NetSocket()
{
	// Pick a range of four UDP ports to try to allocate by default,
	// computed based on my Unix user ID.
	// This makes it trivial for up to four Peerster instances per user
	// to find each other on the same host,
	// barring UDP port conflicts with other applications
	// (which are quite possible).
	// We use the range from 32768 to 49151 for this purpose.
	myPortMin = 32768 + (getuid() % 4096)*4;
	myPortMax = myPortMin + 3;
	


	
	messageIdCounter = 1;
	

	dispatcher = new Dispatcher(&fs, this);
	QObject::connect(this, SIGNAL(toDispatcher(const QMap<QString, QVariant>&)),
			 dispatcher, SLOT(processRequest(const QMap<QString, QVariant>&)));

	QObject::connect(dispatcher, SIGNAL(sendNeighbor(const QMap<QString, QVariant>&, quint32)),
			 this, SLOT(sendNeighbor(const QMap<QString, QVariant> &, quint32)));



	anythingHot = false;

	antiEntropyTimer.setSingleShot(false);
	antiEntropyTimer.start(10000);
	
	routeRumorTimer.setSingleShot(false);
	routeRumorTimer.start(60000);
	
	qsrand((QDateTime::currentDateTime()).toTime_t());

	QObject::connect(&routeRumorTimer, SIGNAL(timeout()),
			 this, SLOT(routeRumorTimeout()));
	
	QObject::connect(this, SIGNAL(startRouteRumorTimer(int)),
			 &routeRumorTimer, SLOT(start(int)));

	QObject::connect(this, SIGNAL(readyRead()),
			 this, SLOT(readData()));

	QObject::connect(this, SIGNAL(startRumorTimer(int)),
			 &rumorTimer, SLOT(start(int)));

	QObject::connect(&rumorTimer, SIGNAL(timeout()),
			 this, SLOT(newRumor()));
	
	
	
	QObject::connect(&antiEntropyTimer, SIGNAL(timeout()),
			 this, SLOT(processAntiEntropyTimeout()));
			 
	
}

void 
NetSocket::processFiles(const QStringList &files)
{
  fs.IndexFiles(files);
}

void NetSocket::routeRumorTimeout()
{
  QVariantMap udpBodyAsMap;
  routeRumorTimer.stop();
        
  // Put the values in the map.
  udpBodyAsMap["SeqNo"] = messageIdCounter++;
  udpBodyAsMap["Origin"] = myNameString;
    
  if (updateVector(udpBodyAsMap, false)){
    
    broadcastMessage(udpBodyAsMap);
    emit startRouteRumorTimer(60000); 
  }
  
  else {
    qDebug() << "NetSocket::gotSendMessage -- OUT OF ORDER MESSAGE FROM MYSELF: COMMIT SUICIDE";
    // *((int *)NULL) = 1;
  }  
}


void NetSocket::addHost(const QString& s)
{
  neighborList.addHost(s);
}

// After receiving a time-out from the antientropy timer, send 
// my vector clock to a random neighbor.
void NetSocket::processAntiEntropyTimeout()
{
  
  QPair<QHostAddress, quint16> neighbor = neighborList.randomNeighbor();
  sendStatusMessage(neighbor.first, neighbor.second);

}



bool NetSocket::bind(QList<QString> &paxosNodes)
{
	// Try to bind to each of the range myPortMin..myPortMax in turn.
  quint16 qMyPortMin = (quint16) myPortMin;
  quint16 qMyPortMax = (quint16) myPortMax;

  // Find and register all the neighbors. For the time being they are just those on the same
  // host on different ports.
  for (quint16 p = qMyPortMin; p <= qMyPortMax; p++) {
		if (QUdpSocket::bind(p)) {

		  /*			////qDebug() << "bound to UDP port " << p;

			if (p == qMyPortMin){
			  

			  neighborList.addNeighbor(QHostAddress::LocalHost, p + 1);
		
			  
			}

			else if (p == qMyPortMin + 1){


			  neighborList.addNeighbor(QHostAddress::LocalHost, p - 1);
			  neighborList.addNeighbor(QHostAddress::LocalHost, p + 1);

			}

			else if (p == qMyPortMin + 2){
	
			  neighborList.addNeighbor(QHostAddress::LocalHost, p - 1);
			  neighborList.addNeighbor(QHostAddress::LocalHost, p + 1);

			}

			else if (p == qMyPortMin + 3){

			  neighborList.addNeighbor(QHostAddress::LocalHost, p - 1);
			}
			
		  */	
		  
		  qDebug() << "port: " << p;
			for (quint16 q = qMyPortMin; q <= qMyPortMax; q++) {
			  
			  if (p != q){			    
			    //neighbors.append(QPair<QHostAddress, quint16>(QHostAddress::LocalHost, q));
			    neighborList.addNeighbor(QHostAddress::LocalHost, q);
			    
			  }
			  			  
			}
			
		  
			noForward = false;	

			QStringList args = QCoreApplication::arguments();
			
			int max = args.count();
			bool inPaxos = false;
			bool donePaxos = false;
			
			qDebug() << args.count();
			
			for(int i = 1; i < max; ++i){
			  
			  if (!inPaxos && !donePaxos && (args[i] == "-paxos-nodes"))
			    inPaxos = true;

			  else if(inPaxos){	

			    if (args[i][0] == '-'){
			      inPaxos = false;
			      donePaxos = true;
			    }	
			    else{
			      qDebug() << args[i];
			      paxosNodes.append(args[i]);			    
			    }
			  }		      
			   
			  else if (args[i] == "-noforward"){
			    
			    //qDebug() << "No Forwarding!!!";
			    noForward = true;
			  }
			  ////qDebug() << args[i];
			  /*
			  else
			  addHost(args[i]);*/
			}


			qDebug() << "Num paxos nodes =" << paxosNodes.count();
			if(paxosNodes.count() == 0){
			  
			  qDebug() << "Node identifiers not specified, exiting...";
			  exit(0);
			}
			
			router = new Router(this, noForward);
			
			myNameString = paxosNodes[0];

			/*
			QTextStream stream(&myNameString);
			
			stream << QUuid::createUuid();
			
			stream << (QDateTime::currentDateTime()).toTime_t();
			*/
			
			myNameVariant = QVariant(myNameString);
			
			//qDebug() << p;
			
			router->me = myNameString;
			
			fileRequests = new FileRequests(myNameString);
			

			connect(fileRequests, SIGNAL(sendDownloadMsg(const QMap<QString, QVariant>&, const QString&)),
				router, SLOT(sendMap(const QMap<QString, QVariant>&, const QString&)));

			connect(router, SIGNAL(blockRequest(const QMap<QString, QVariant> &)),
				dispatcher, SLOT(processRequest(const QMap<QString, QVariant> &)));

			connect(router, SIGNAL(toFileRequests(const QMap<QString, QVariant> &)),
				fileRequests, SLOT(processReply(const QMap<QString, QVariant> &)));
			
			connect(dispatcher, SIGNAL(reply(const QMap<QString, QVariant>&, const QString &)),
				router, SLOT(sendMap(const QMap<QString, QVariant>&, const QString&)));

			connect(fileRequests, SIGNAL(broadcastRequest(const QMap<QString, QVariant> &)),
				this, SLOT(broadcastMessage(const QMap<QString, QVariant>&)));

			////qDebug() << "Finished intialization!!!";
			return true;
		}
	}

	////qDebug() << "Oops, no ports in my default range " << myPortMin
  //<< "-" << myPortMax << " available";
	return false;
}


// Received a message from the dialog.
// Construct the message as a QVariantMap and 
// call the function to handle received rumors.
void NetSocket::gotSendMessage(const QString &s)
{

  QVariantMap udpBodyAsMap;

    
  // Put the values in the map.
  udpBodyAsMap["ChatText"] = s;
  udpBodyAsMap["SeqNo"] = messageIdCounter++;
  udpBodyAsMap["Origin"] = myNameString;
  

  
  if (updateVector(udpBodyAsMap, true)){
        
    newRumor();
  }
  
  else {
    qDebug() << "NetSocket::gotSendMessage -- OUT OF ORDER MESSAGE FROM MYSELF: COMMIT SUICIDE";
    //    *((int *)NULL) = 1;
  }

}

// Send status message to the given address:port combination.
// Reads the current state of the vector clock.
void NetSocket::sendStatusMessage(QHostAddress address, quint16 port)
{
  QVariantMap udpBodyAsMap;
  

  udpBodyAsMap["Want"] = vectorClock;

  QByteArray arr;
  
  QDataStream s(&arr, QIODevice::Append);
  s << udpBodyAsMap;
  ////qDebug() << "NetSocker::sendStatusMessage " << udpBodyAsMap["Want"];
  this->writeDatagram(arr, address, port);
  ////qDebug() << "NetSocket:sendStatusMessage -- finished sending status to " << address << " " << port;
  
}



bool
NetSocket::expectedRumor(const QVariantMap& rumor, QString *origin, quint32* expected)
{
  *origin = rumor["Origin"].toString();
  

  if (vectorClock.contains(*origin)){

    
    *expected = (vectorClock[*origin]).toUInt();
    ////qDebug() << "NetSocket::newRumor -- Contain entry for " << *origin << " expect " << *expected;
  }
  else{
    *expected = 1;
    ////qDebug() << "NetSocket::newRumor -- Don't contain entry for " << *origin << " expect " << 1;
  }
  
  ////qDebug() << "NetSocket::newRumor -- got " << " " << rumor["SeqNo"].toUInt();
  
  return (*expected) == rumor["SeqNo"].toUInt();
}

bool 
NetSocket::updateVector(const QVariantMap& rumor, bool isRumorMessage)
{
  QString origin;
  quint32 expected;
  if (expectedRumor(rumor, &origin, &expected)){
    
    QByteArray arr;
      
    QDataStream stream(&arr, QIODevice::Append);
    stream << rumor;

    anythingHot = true;
    hotMessage = rumor;
      
    ////qDebug() << "NetSocket::newRumor -- yay, in-order message!!!";
      


    vectorClock[origin] = expected + 1;
	
    if (expected == 1){
      QList<QVariantMap> temp;
      messages[origin] = temp;
      messages[origin].append(EMPTY_VARIANT_MAP);
    }
    messages[origin].append(rumor);

    if (isRumorMessage){
      emit receivedMessage ((rumor["ChatText"]).toString());    	
    }
    
    
    return true;
  }
  
  return false;
}

// We might have received a new rumor.
// a) If it is from the dialog, then it is definitely new.
//
// b) If it is from the network, then it might not be new,
//    we have to account for this by checking the expected 
//    value from the vector clock with the sequence number
//    in the message.
//
// c) If we have a new message, start rumor mongering.
// 
// d) Only this function manipulates the vector clock and messages.
//
// e) Only this function sets anythingHot to true.
void NetSocket::newRumor()
{

  rumorTimer.stop();  
  
  if(anythingHot){
    if (!noForward || !hotMessage.contains("ChatText")){

      QPair<QHostAddress, quint16> neighbor = neighborList.randomNeighbor();

      this->writeDatagram(Helper::SerializeMap(hotMessage), 
			  neighbor.first, 
			  neighbor.second);   
    
      emit startRumorTimer(2000);
    }
  }
}


// Some utility methods to keep track of which neighbors we have already sent 
// messages to for the same hotmessage. If we change hotmessages, then we have to clean up!!!
// WE ONLY CLEAN UP IN newRumor!!!
/*
void NetSocket::excludeNeighbor(quint32 port)
{
  for (int i = 0; i < neighbors.count(); ++i){
    
    if ((neighbors[i]).second == port){
      neighborsVisited->insert(i);
      return;
    }

  }
}

*/




// If none of the elements are bigger, the string is empty.
int NetSocket::tryFindFirstBigger(const QVariantMap& map1, const QVariantMap& map2, QString* key)
{
  QList<QString> keys = map1.keys();
  for(int i = 0; i < keys.count(); ++i){
    
    if (map2.contains(keys[i])){
      
      //////qDebug() << "NetSocket::tryFindFirstBigger -- first if ok";
      // both have the key, but map1's is higher: Success!!!
      if ((map1[keys[i]].toUInt() > map2[keys[i]].toUInt()) && 
	  (map1[keys[i]].toUInt() > 1)){

	//////qDebug() << "NetSocket::tryFindFirstBigger -- inner if ok";
	*key = keys[i];
	return map2[keys[i]].toUInt();
      }
      
      else
	continue;
    } 


    // map2 does not have the key: Success!!!
    else if (map1[keys[i]].toUInt() == 1){
     
      continue;
    }
    
    else {
      
      *key = keys[i];
      return 1;
    }

  }
  
  return -1;
}


bool NetSocket::checkVector(const QVariantMap& vect)
{
  QVariant temp = vect["Want"];

  if (temp.type() == QVariant::Map){
    
    QMap<QString, QVariant> other_vector = temp.toMap();
    QList<QString> keys = other_vector.keys();
    for(int i = 0; i < keys.count(); ++i){
      
      bool isInt;
      quint32 value = other_vector[keys[i]].toUInt(&isInt);
      if (isInt){
	
	if (value < 1){
	  
	  return false;
	}
      }
      
      else{ // if (isInt)
	return false;
      }
    }
    
    return true;
  }

  else{ //   if (temp.type() == QVariant::Map)
    return false;
  }
  

}

void NetSocket::addUnknownOrigins(const QVariantMap &message)
{
  QList<QString> keys = message.keys();
  int len = keys.count();

  for (int i = 0; i < len; ++i){
    
    if (!(vectorClock.contains(keys[i]))){
      
      vectorClock[keys[i]] = 1;
    }
  }
  
}

// Called when we receive a new status message.
// 
// Handles *both* messages due to rumor mongering and anti-entropy.
//
// a) If we have nothing hot, then it's safe to rumor monger
//    with just the host that sent us the status message.
//
// b) If anythingHot is true, we have to make sure that we also accommodate for
//    rumormongering with all neighbors.

void NetSocket::newStatus(const QVariantMap& message,
			   const QHostAddress& senderAddress, 
			   const quint16& port)
{
  rumorTimer.stop();
  QString ans;
  int required;

  
  
  if (checkVector(message)){
  
    
    addUnknownOrigins(message["Want"].toMap());
    ////qDebug() << "NetSocket::newStatus " << message["Want"];

    // Our vector is bigger!!!
    if ((required = tryFindFirstBigger(vectorClock, message["Want"].toMap(), &ans)) != -1){

      if (!noForward || !messages[ans][required].contains("ChatText")){

	this->writeDatagram(Helper::SerializeMap(messages[ans][required]), 
			    senderAddress, 
			    port);
      }

      emit startRumorTimer(2000);   
      return;
    }

    // Her's is bigger :(
    else if ((required = tryFindFirstBigger(message["Want"].toMap(), vectorClock, &ans)) != -1){
    
      ////qDebug() << "NetSocket::newStatus -- her's is bigger!!!";

      sendStatusMessage(senderAddress, port);
      ////qDebug() << "NetSocket::newStatus -- wrote our status!!!";
      ////qDebug() << '\n';
      return;
    }  
  
  }
  // Tie: propagate hot message

  if (anythingHot){
    if (!noForward || !hotMessage.contains("ChatText")){
    
    
      ////qDebug() << "NetSocket::newStatus -- tie!!!";
      // Flip a coin
      if (qrand() % 2){
      
      
	////qDebug() << "NetSocket::newStatus -- got heads! try to find next neighbor";
	QPair<QHostAddress, quint16> neighbor = neighborList.randomNeighbor();
      
	////qDebug() << "NetSocket::newStatus -- send to next neighbor!!!";
      
	////qDebug() << hotMessage;
      
	this->writeDatagram(Helper::SerializeMap(hotMessage),
			    neighbor.first, 
			    neighbor.second);
	////qDebug() << "NetSocket::newStatus -- sent message!!!";
	emit startRumorTimer(2000);

      }
      else {
	////qDebug() << "NetSocket::newStatus -- got tails! done!!!";
	////qDebug() << '\n';
	anythingHot = false; 
      }
    }
  }
}
  

void NetSocket::broadcastMessage(const QMap<QString, QVariant> & msg)
{
  QList<QPair<QHostAddress, quint16> > neighbors = neighborList.getAllNeighbors();
  
  int len = neighbors.count();
  for(int i = 0; i < len; ++i){
    
    this->writeDatagram(Helper::SerializeMap(msg), 
			neighbors[i].first, 
			neighbors[i].second);
  }
}

quint32
NetSocket::numNeighbors()
{
  return neighborList.getAllNeighbors().count();
}

void NetSocket::sendNeighbor(const QVariantMap &msg, quint32 neighbor)
{
  QPair<QHostAddress, quint16> addr = neighborList.getAllNeighbors()[neighbor];
  
  this->writeDatagram(Helper::SerializeMap(msg),
		      addr.first,
		      addr.second);
}



// Read data from the network and redirect the message for analysis to 
// either newRumor or newStatus.

void NetSocket::readData()
{
  int count = 0;
  while(this->hasPendingDatagrams()){
  
    if (count > 0){
      //qDebug() << "looping ... " << count;
    }
    const qint64 size = this->pendingDatagramSize();
    if (size == -1){
      ////qDebug() << "NetSocket::readData() -- Error signalled for new data, but nothing present";
      ////qDebug() << '\n';
      return;
    }
    char data[size];

    QHostAddress senderAddress;
    quint16 port = 0;
  
    if (size != this->readDatagram(data, size, &senderAddress, &port)){
      ////qDebug() << "NetSocket::readData() -- Error reading data from socket. Sizes don't match!!!";
    }

    ////qDebug() << "NetSocket::readData() -- just received a datagram!!!";

    neighborList.addNeighbor(senderAddress, port);

    QByteArray arr(data, size);
    QVariantMap items;
  
    QDataStream stream(arr);
    stream >> items;
  
    ////qDebug() << "Checking if message is rumor or status..."; //Debug Message to make sure we receive the right stuff!!!


    // Rumor message:
    if (items.contains("Origin") &&
	items.contains("SeqNo")){
   
    
      if (items.contains("LastIP") && items.contains("LastPort")){
      
	//qDebug() << items;
	QHostAddress holeIP(items["LastIP"].toInt());
	quint16 holePort = items["LastPort"].toInt();
	neighborList.addNeighbor(holeIP, holePort);
      }
    
      router->processRumor(items, senderAddress, port);    

      ////qDebug() << "Rumor!!!";
      ////qDebug() << items;
    

    
      items["LastIP"] = senderAddress.toIPv4Address();
      items["LastPort"] = port;
    
      bool isRumorMessage = items.contains("ChatText");
      if (updateVector(items, isRumorMessage)){

	sendStatusMessage(senderAddress, port);
      
	if (isRumorMessage)
	  newRumor();
	else
	  broadcastMessage(items);      
      }
    }


    // Status message:
    else if (items.contains("Want") && !(items.contains("ChatText"))){
					

      ////qDebug() << "Status!!!";
      newStatus(items, senderAddress, port);
    
    }

  
    else if(items.contains("Dest") &&
	    items.contains("Origin") &&
	    items.contains("HopLimit")){
	
      //qDebug() << "private message";

      router->receiveMessage(items);
    
      ////qDebug() << "Unexpected Message";
      ////qDebug() << items;
    }

    else if (items.contains("Origin") &&
	     items.contains("Search") &&
	     items.contains("Budget")){
      qDebug() << "sending to dispatcher";
      emit toDispatcher(items);
    }
  
    ++count;
  }
    
	   
  ////qDebug() << '\n';
}

// END: NetSocket


QDataStream &
operator<< (QDataStream &out, const ProposalNumber &myObj)
{
  out << myObj.number;
  out << myObj.name;
  return out;
}
  
QDataStream &
operator>> (QDataStream &in, ProposalNumber &myObj)
{
  quint64 my_number;
  QString my_name;

  in >> my_number;
  in >> my_name;
  
  myObj.name = my_name;
  myObj.number = my_number;
  return in;
}


int main(int argc, char **argv)
{
  for(int i = 0; i < argc; ++i){
    
    
  }


  
  qRegisterMetaType<ProposalNumber>("ProposalNumber");
  
  qRegisterMetaTypeStreamOperators<ProposalNumber>("ProposalNumber");
  
	// Initialize Qt toolkit
	QApplication app(argc,argv);

	// Create an initial chat dialog window


	QCA::Initializer qcainit;

	
	// Create a UDP network socket
	NetSocket sock;
	QList<QString> paxosList;
	if (!sock.bind(paxosList))
		exit(1);
	
	qDebug() << "num nodes paxos =" << paxosList.count();
	
	QTabWidget mainWidget;
	
	ChatDialog dialog(sock.router);
	FileDialog fileDialog(sock.fileRequests);
	PaxosDialog paxosDialog(sock.router, paxosList);
	//dialog.show();
	
	mainWidget.addTab(&dialog, "Chats");
	mainWidget.addTab(&fileDialog, "Transfers");
	mainWidget.addTab(&paxosDialog, "Paxos");
	mainWidget.show();


	QObject::connect(&fileDialog, SIGNAL(indexFiles(const QStringList&)),
			 &sock, SLOT(processFiles(const QStringList&)));
	QObject::connect(&dialog, SIGNAL(sendMessage(const QString&)),
			 &sock, SLOT(gotSendMessage(const QString&)));

	
	QObject::connect(sock.router, SIGNAL(privateMessage(const QString&, const QString&)),
			 &dialog, SLOT(newPrivateMessage(const QString&, const QString&)));

	QObject::connect(&dialog, SIGNAL(sendPrivateMessage(const QString&, const QString&)),
			 sock.router, SLOT(sendMessage(const QString&, const QString&)));
	

	QObject::connect(&sock, SIGNAL(receivedMessage(const QString&)),
			 &dialog, SLOT(gotNewMessage(const QString&)));


	QObject::connect(&dialog, SIGNAL(addPeer(const QString&)),
			 &sock, SLOT(addHost(const QString&)));

	QTimer::singleShot(0, &sock, SLOT(routeRumorTimeout()));



	
	// Enter the Qt main loop; everything else is event driven
	return app.exec();
}

