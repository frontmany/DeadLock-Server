
class WorkerUI {
public:
	virtual void onStatusReceive() = 0;
	virtual void onMessageReceive() = 0;
	virtual void onMessageReadConfirmationReceive() = 0;
};